// video.cpp - FMV backend: ffmpeg (Cinepak) into a GL texture.
//
// Windows hands the AVI to MCI, which paints the movie into the game window
// itself. There is no such service here, so this backend decodes with ffmpeg,
// uploads each frame to a MarniDX texture and draws it as a full-screen quad.
//
// Clock: movie audio is decoded up front. Native SDL uses the mixer cursor;
// Android/browser output uses a scheduled WebAudio source and its context
// timeline, independent of delayed SDL callbacks. With no audio, wall time
// stands in.
//
// The state machine's cut points are times; this backend turns them into
// frames with the stream's own rate, so a source-timed or PC-retimed movie
// still cuts at the same moment.
#include "../platform.h"

#include "../../Globals.h"
#include "../../marni/MarniDX.h"
#include "../../marni/MarniSystem.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
extern "C" void re1_web_hd_dynamic(int dynamic);
#endif

// audio.cpp - the movie audio stream (see its note on s_streamPcm).
int  plat_audio_stream_rate(void);
void plat_audio_stream_play(const short* pcm, long totalFrames,
                            long startFrame, long endFrame);
void plat_audio_stream_stop(void);
long plat_audio_stream_pos(void);

namespace {

PlatVideoOverlayCallback s_overlayCallback = NULL;

AVFormatContext* s_fmt     = NULL;
AVCodecContext*  s_vctx    = NULL;
AVCodecContext*  s_actx    = NULL;
SwrContext*      s_swr     = NULL;
int              s_vstream = -1;
int              s_astream = -1;
AVPacket*        s_pkt     = NULL;
AVFrame*         s_frame   = NULL;

double s_fps    = 10.0;
int    s_width  = 0;
int    s_height = 0;

unsigned char* s_rgba      = NULL;   // last decoded frame, RGBA
int            s_rgbaPitch = 0;

MarniHandle s_tex  = MARNI_NULL_HANDLE;
int         s_texW = 0;
int         s_texH = 0;

int  s_frameIndex   = -1;     // index of the frame in s_rgba (absolute)
int  s_baseFrame    = 0;      // frame the current segment started at
int  s_playToFrame  = 0;      // 0 = play to the end
BOOL s_active       = FALSE;
BOOL s_endEvent     = FALSE;
BOOL s_eof          = FALSE;
BOOL s_flushed      = FALSE;   // the decoder's NULL-packet flush was sent
DWORD s_startTicks  = 0;      // wall clock, used only when the movie has no audio

short* s_audio        = NULL; // whole movie, S16 stereo at the device rate
long   s_audioFrames  = 0;
long   s_audioStartSample = 0;   // first sample of the current segment
BOOL   s_useAudio     = FALSE;

// ---------------------------------------------------------------------------
// Teardown
// ---------------------------------------------------------------------------
void FreeAll(void)
{
    plat_audio_stream_stop();

    if (s_tex != MARNI_NULL_HANDLE) {
        MarniDX* dx = Marni_DX();
        if (dx != NULL) dx->DestroyTexture(s_tex);
        s_tex = MARNI_NULL_HANDLE;
    }
    s_texW = s_texH = 0;

    free(s_rgba);      s_rgba = NULL;      s_rgbaPitch = 0;
    free(s_audio);     s_audio = NULL;     s_audioFrames = 0;  s_useAudio = FALSE;

    if (s_swr)    { swr_free(&s_swr); }
    if (s_frame)  { av_frame_free(&s_frame); }
    if (s_pkt)    { av_packet_free(&s_pkt); }
    if (s_vctx)   { avcodec_free_context(&s_vctx); }
    if (s_actx)   { avcodec_free_context(&s_actx); }
    if (s_fmt)    { avformat_close_input(&s_fmt); }

    s_vstream = -1;
    s_astream = -1;
    s_frameIndex = -1;
    s_baseFrame = 0;
    s_playToFrame = 0;
    s_eof = FALSE;
    s_flushed = FALSE;
    s_active = FALSE;
    s_endEvent = FALSE;
}

// ---------------------------------------------------------------------------
// DecodeAllAudio - decode the whole audio track into one S16 stereo buffer.
//
// Pre-decoding is what makes the audio usable as a clock: the mixer needs a
// buffer it can read at its own pace, and a movie is short enough to hold in
// RAM (the longest is a few minutes).
// ---------------------------------------------------------------------------
void DecodeAllAudio(void)
{
    if (s_actx == NULL || s_astream < 0) return;

    const int outRate = plat_audio_stream_rate();
    const int outChannels = 2;

    // Resample to the device's format/rate. Movies are 22050 Hz stereo here,
    // but this keeps the backend honest if a file is not.
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&s_swr, &outLayout, AV_SAMPLE_FMT_S16, outRate,
                            &s_actx->ch_layout, s_actx->sample_fmt,
                            s_actx->sample_rate, 0, NULL) < 0 ||
        swr_init(s_swr) < 0) {
        swr_free(&s_swr);
        return;
    }

    long capacity = 0;
    long written = 0;
    AVPacket* pkt = av_packet_alloc();
    AVFrame*  frm = av_frame_alloc();
    if (pkt == NULL || frm == NULL) { av_packet_free(&pkt); av_frame_free(&frm); return; }

    // Resample one decoded audio frame into the output buffer, growing it.
    // Returns FALSE when the buffer could not be grown (out of memory).
    struct Sink {
        long* written;
        long* capacity;
        short** buf;
        int outRate;
        int outChannels;
        SwrContext* swr;
        AVCodecContext* actx;
        BOOL Store(AVFrame* f) {
            const int maxOut = (int)av_rescale_rnd(
                swr_get_delay(swr, actx->sample_rate) + f->nb_samples,
                outRate, actx->sample_rate, AV_ROUND_UP);
            if (*written + maxOut + 1 > *capacity) {
                long want = (*written + maxOut + 1) * 2;
                short* grown = (short*)realloc(*buf, (size_t)want * outChannels * sizeof(short));
                if (grown == NULL) return FALSE;
                *buf = grown;
                *capacity = want;
            }
            uint8_t* out[1] = { (uint8_t*)(*buf + (size_t)*written * outChannels) };
            int got = swr_convert(swr, out, maxOut,
                                  (const uint8_t**)f->extended_data, f->nb_samples);
            if (got > 0) *written += got;
            return TRUE;
        }
    } sink = { &written, &capacity, &s_audio, outRate, outChannels, s_swr, s_actx };

    BOOL ok = TRUE;
    while (ok && av_read_frame(s_fmt, pkt) >= 0) {
        if (pkt->stream_index != s_astream) { av_packet_unref(pkt); continue; }

        int ret = avcodec_send_packet(s_actx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            // Input queue full: drain it, then retry the same packet.
            while (avcodec_receive_frame(s_actx, frm) == 0) {
                if (!sink.Store(frm)) { ok = FALSE; break; }
                av_frame_unref(frm);
            }
            ret = avcodec_send_packet(s_actx, pkt);
        }
        av_packet_unref(pkt);
        if (!ok) break;
        if (ret < 0) continue;

        while (avcodec_receive_frame(s_actx, frm) == 0) {
            if (!sink.Store(frm)) { ok = FALSE; break; }
            av_frame_unref(frm);
        }
    }

    // Flush the resampler's tail.
    if (ok && written + 4096 <= capacity) {
        uint8_t* out[1] = { (uint8_t*)(s_audio + (size_t)written * outChannels) };
        int got = swr_convert(s_swr, out, 4096, NULL, 0);
        if (got > 0) written += got;
    }

    av_packet_free(&pkt);
    av_frame_free(&frm);

    s_audioFrames = written;
    s_useAudio = (written > 0) ? TRUE : FALSE;

    // Rewind the demuxer to the start for the video pass.
    av_seek_frame(s_fmt, s_vstream, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(s_vctx);
}

// ---------------------------------------------------------------------------
// ConvertAndStore - pack the decoder's frame into the RGBA buffer.
//
// The game's movies are all Cinepak, whose decoder emits packed RGB; the
// conversion is a per-pixel shuffle, so swscale (and its i386 package) is not
// needed. Anything unexpected is reported once and skipped.
// ---------------------------------------------------------------------------
BOOL ConvertAndStore(void)
{
    if (s_width <= 0 || s_height <= 0) return FALSE;

    if (s_rgba == NULL) {
        s_rgbaPitch = s_width * 4;
        s_rgba = (unsigned char*)malloc((size_t)s_rgbaPitch * s_height);
        if (s_rgba == NULL) return FALSE;
    }

    const int srcStride = s_frame->linesize[0];
    const unsigned char* src = s_frame->data[0];
    unsigned char* dst = s_rgba;

    switch (s_frame->format) {
    case AV_PIX_FMT_RGB24:
        for (int y = 0; y < s_height; ++y) {
            const unsigned char* s = src + (size_t)y * srcStride;
            unsigned char* d = dst + (size_t)y * s_rgbaPitch;
            for (int x = 0; x < s_width; ++x) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
                s += 3; d += 4;
            }
        }
        return TRUE;
    case AV_PIX_FMT_BGR24:
        for (int y = 0; y < s_height; ++y) {
            const unsigned char* s = src + (size_t)y * srcStride;
            unsigned char* d = dst + (size_t)y * s_rgbaPitch;
            for (int x = 0; x < s_width; ++x) {
                d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 255;
                s += 3; d += 4;
            }
        }
        return TRUE;
    case AV_PIX_FMT_RGBA:
        for (int y = 0; y < s_height; ++y) {
            memcpy(dst + (size_t)y * s_rgbaPitch, src + (size_t)y * srcStride,
                   (size_t)s_width * 4);
        }
        return TRUE;
    case AV_PIX_FMT_BGRA:
        for (int y = 0; y < s_height; ++y) {
            const unsigned char* s = src + (size_t)y * srcStride;
            unsigned char* d = dst + (size_t)y * s_rgbaPitch;
            for (int x = 0; x < s_width; ++x) {
                d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
                s += 4; d += 4;
            }
        }
        return TRUE;
    case AV_PIX_FMT_YUV420P:
        {
            const unsigned char* yp = s_frame->data[0];
            const unsigned char* up = s_frame->data[1];
            const unsigned char* vp = s_frame->data[2];
            const int yStride = s_frame->linesize[0];
            const int uStride = s_frame->linesize[1];
            const int vStride = s_frame->linesize[2];
            for (int y = 0; y < s_height; ++y) {
                const unsigned char* yRow = yp + (size_t)y * yStride;
                const unsigned char* uRow = up + (size_t)(y / 2) * uStride;
                const unsigned char* vRow = vp + (size_t)(y / 2) * vStride;
                unsigned char* d = dst + (size_t)y * s_rgbaPitch;
                for (int x = 0; x < s_width; ++x) {
                    const int yy = yRow[x] - 16;
                    const int uu = uRow[x / 2] - 128;
                    const int vv = vRow[x / 2] - 128;
                    const int r = (298 * yy + 409 * vv + 128) >> 8;
                    const int g = (298 * yy - 100 * uu - 208 * vv + 128) >> 8;
                    const int b = (298 * yy + 516 * uu + 128) >> 8;
                    d[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
                    d[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
                    d[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
                    d[3] = 255;
                    d += 4;
                }
            }
        }
        return TRUE;
    default:
        fprintf(stderr, "[VIDEO] unsupported pixel format %d\n", s_frame->format);
        return FALSE;
    }
}

// ---------------------------------------------------------------------------
// DecodeNextVideoFrame - advance the video decoder by one frame.
//
// The send/receive handshake has to be drained properly: a decoder whose input
// queue is full returns EAGAIN from send_packet and expects a receive first,
// and once the file ends the NULL-packet flush must happen exactly once.
// Getting either wrong spins forever - which is exactly what an early version
// of this did, roughly once in four runs.
// ---------------------------------------------------------------------------
BOOL DecodeNextVideoFrame(void)
{
    if (s_eof) return FALSE;

    for (;;) {
        int ret = avcodec_receive_frame(s_vctx, s_frame);
        if (ret == 0) {
            // Track the frame number from the timestamp. Counting decodes
            // instead only works from the start of the file: after a seek the
            // decoder starts at the preceding keyframe, so a count would land
            // `frame` pictures past the target - which is what made the
            // prologue cut run off the end of the movie and go black while the
            // audio, seeked by sample count, carried on correctly.
            AVStream* st = s_fmt->streams[s_vstream];
            if (s_frame->pts != AV_NOPTS_VALUE && st->time_base.den > 0) {
                const double sec = (double)s_frame->pts * av_q2d(st->time_base);
                s_frameIndex = (int)(sec * s_fps + 0.5);
            } else {
                s_frameIndex++;
            }
            return ConvertAndStore();
        }
        if (ret == AVERROR_EOF) { s_eof = TRUE; return FALSE; }
        if (ret != AVERROR(EAGAIN)) { s_eof = TRUE; return FALSE; }

        // The decoder wants more input. If the file is exhausted and the flush
        // packet has already gone in, there is nothing left to produce.
        if (s_flushed) { s_eof = TRUE; return FALSE; }

        int readRet = av_read_frame(s_fmt, s_pkt);
        while (readRet >= 0 && s_pkt->stream_index != s_vstream) {
            av_packet_unref(s_pkt);
            readRet = av_read_frame(s_fmt, s_pkt);
        }
        if (readRet < 0) {
            s_flushed = TRUE;
            avcodec_send_packet(s_vctx, NULL);
            continue;
        }

        ret = avcodec_send_packet(s_vctx, s_pkt);
        av_packet_unref(s_pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN)) { s_eof = TRUE; return FALSE; }
        // EAGAIN means "receive first" - loop back and do exactly that.
    }
}

// ---------------------------------------------------------------------------
// SeekTo - position the video decoder (and the audio cursor) at `frame`.
// ---------------------------------------------------------------------------
BOOL SeekTo(int frame)
{
    if (frame < 0) frame = 0;

    const int64_t ts = (int64_t)((double)frame / s_fps * (double)AV_TIME_BASE);
    av_seek_frame(s_fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(s_vctx);
    s_eof = FALSE;
    s_flushed = FALSE;
    s_frameIndex = -1;

    // Decode forward to the requested frame so s_rgba holds it. The decoder
    // restarts at the keyframe before the target, so this walks until the
    // timestamp-derived index reaches it (frame 0 needs one decode).
    while (s_frameIndex < frame && !s_eof) {
        if (!DecodeNextVideoFrame()) break;
    }

    if (s_useAudio) {
        const long startSample = (long)((double)frame / s_fps * plat_audio_stream_rate());
        const long endSample = (s_playToFrame > 0)
            ? (long)((double)s_playToFrame / s_fps * plat_audio_stream_rate())
            : s_audioFrames;
        s_audioStartSample = startSample;
        plat_audio_stream_play(s_audio, s_audioFrames, startSample, endSample);
    }

    // The clock restarts with the segment, but frame numbers stay absolute:
    // a resumed segment begins at `frame`, not at 0.
    s_baseFrame = frame;
    s_startTicks = plat_time_ms();
    return TRUE;
}

// ---------------------------------------------------------------------------
// Present - upload the current frame and draw it over the whole back buffer.
// ---------------------------------------------------------------------------
void Present(void)
{
    if (s_rgba == NULL || s_width <= 0 || s_height <= 0) return;

    MarniDX* dx = Marni_DX();
    if (dx == NULL) return;

    if (s_tex == MARNI_NULL_HANDLE || s_texW != s_width || s_texH != s_height) {
        if (s_tex != MARNI_NULL_HANDLE) dx->DestroyTexture(s_tex);
        int ow = 0, oh = 0;
#ifdef __EMSCRIPTEN__
        // Movies are streaming textures, never HD pack still images.
        re1_web_hd_dynamic(1);
#endif
        s_tex = dx->CreateTexture(s_width, s_height, 32, s_rgba, &ow, &oh);
#ifdef __EMSCRIPTEN__
        re1_web_hd_dynamic(0);
#endif
        s_texW = s_width;
        s_texH = s_height;
        if (s_tex == MARNI_NULL_HANDLE) return;
    } else {
        dx->UpdateTexturePixels(s_tex, s_rgba, s_width, s_height, 32);
    }

    DWORD bw = 0, bh = 0;
    dx->GetBackBufferSize(&bw, &bh);
    if (bw == 0 || bh == 0) return;

    dx->Clear(0.0f, 0.0f, 0.0f, 1.0f);
    const int frameMs = s_frameIndex > 0
        ? (int)((double)s_frameIndex * 1000.0 / s_fps + 0.5) : 0;
    if (s_overlayCallback == NULL || !s_overlayCallback(s_tex, frameMs)) {
        dx->DrawSprite(0.0f, 0.0f, (float)bw, (float)bh,
                       0.0f, 0.0f, 1.0f, 1.0f,
                       0xFFFFFFFFu, s_tex, MARNI_SAMPLER_POINT, MARNI_BLEND_DISABLE);
    }
    dx->Present();
}

}  // namespace

// ===========================================================================
// Backend entry points (see platform.h)
// ===========================================================================

void plat_video_set_overlay_callback(PlatVideoOverlayCallback callback)
{
    s_overlayCallback = callback;
}

BOOL plat_video_init(void)
{
    return TRUE;   // ffmpeg is linked in; nothing to probe
}

BOOL plat_video_open_and_play(const char* path, int playToMs)
{
    FreeAll();
#ifdef __EMSCRIPTEN__
    if (getenv("RE1_SKIP_MOVIES") && strcmp(getenv("RE1_SKIP_MOVIES"), "1") == 0) {
        fprintf(stderr, "[VIDEO] skipped by launcher setting: %s\n", path);
        return FALSE;
    }
    extern void re1_web_video_stage(const char* path);
    re1_web_video_stage(path);
#endif

    if (avformat_open_input(&s_fmt, path, NULL, NULL) < 0) {
        fprintf(stderr, "[VIDEO] ffmpeg could not open %s\n", path);
        FreeAll();
        return FALSE;
    }
    if (avformat_find_stream_info(s_fmt, NULL) < 0) {
        fprintf(stderr, "[VIDEO] no stream info in %s\n", path);
        FreeAll();
        return FALSE;
    }

    s_vstream = av_find_best_stream(s_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    s_astream = av_find_best_stream(s_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (s_vstream < 0) {
        fprintf(stderr, "[VIDEO] no video stream in %s\n", path);
        FreeAll();
        return FALSE;
    }

    // Video decoder
    {
        AVStream* st = s_fmt->streams[s_vstream];
        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec == NULL) { FreeAll(); return FALSE; }
        s_vctx = avcodec_alloc_context3(codec);
        if (s_vctx == NULL) { FreeAll(); return FALSE; }
        if (avcodec_parameters_to_context(s_vctx, st->codecpar) < 0 ||
            avcodec_open2(s_vctx, codec, NULL) < 0) {
            FreeAll();
            return FALSE;
        }
        s_width  = s_vctx->width;
        s_height = s_vctx->height;
        if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) {
            s_fps = av_q2d(st->avg_frame_rate);
        } else if (st->r_frame_rate.num > 0 && st->r_frame_rate.den > 0) {
            s_fps = av_q2d(st->r_frame_rate);
        }
        if (s_fps <= 0.0) s_fps = 10.0;
    }

    // Audio decoder (optional)
    if (s_astream >= 0) {
        AVStream* st = s_fmt->streams[s_astream];
        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec != NULL) {
            s_actx = avcodec_alloc_context3(codec);
            if (s_actx == NULL ||
                avcodec_parameters_to_context(s_actx, st->codecpar) < 0 ||
                avcodec_open2(s_actx, codec, NULL) < 0) {
                avcodec_free_context(&s_actx);
                s_astream = -1;
            }
        } else {
            s_astream = -1;
        }
    }

    s_pkt   = av_packet_alloc();
    s_frame = av_frame_alloc();
    if (s_pkt == NULL || s_frame == NULL) { FreeAll(); return FALSE; }

    if (s_astream >= 0) {
        DecodeAllAudio();
    }

    // The cut point arrives as a time; turn it into this movie's frames (the
    // stream rate is known now, and a converted movie's rate need not be the
    // originals' 10 fps).
    s_playToFrame = (playToMs > 0)
        ? (int)((double)playToMs / 1000.0 * s_fps + 0.5) : 0;
    s_active = TRUE;
    s_endEvent = FALSE;
    SeekTo(0);
    Present();

    fprintf(stderr, "[VIDEO] playing %s (%dx%d, %.2f fps, audio %s)\n",
            path, s_width, s_height, s_fps, s_useAudio ? "on" : "off");
    return TRUE;
}

void plat_video_play_from(int fromMs)
{
    if (s_fmt == NULL) return;
    const int fromFrame = (int)((double)fromMs / 1000.0 * s_fps + 0.5);
    s_playToFrame = 0;
    s_endEvent = FALSE;
    s_active = TRUE;
    SeekTo(fromFrame);
    fprintf(stderr, "[VIDEO] play_from(%dms -> frame %d) index %d eof=%d\n",
            fromMs, fromFrame, s_frameIndex, (int)s_eof);
    Present();
}

void plat_video_stop(void)
{
    s_active = FALSE;
}

void plat_video_close(void)
{
    FreeAll();
}

BOOL plat_video_is_active(void)
{
    return s_active;
}

BOOL plat_video_take_end_event(void)
{
    if (!s_endEvent) return FALSE;
    s_endEvent = FALSE;
    return TRUE;
}

void plat_video_tick(void)
{
    if (!s_active || s_fmt == NULL) return;

    // Target frame: the sound card's cursor when it is trustworthy, the wall
    // clock otherwise. The audio is the better clock (it cannot drift from the
    // sound), but a device that discards its output - a headless ALSA/Pulse
    // fallback - consumes the whole movie in milliseconds, and following that
    // would fast-forward the picture. Accept the audio clock only while it is
    // in step with real time.
    const double wallSeconds = (double)(plat_time_ms() - s_startTicks) / 1000.0;
    double seconds = wallSeconds;
    if (s_useAudio) {
        const long pos = plat_audio_stream_pos();
        if (pos >= 0) {
            const double audioSeconds =
                (double)(pos - s_audioStartSample) / (double)plat_audio_stream_rate();
#ifdef __EMSCRIPTEN__
            // The source's AudioContext clock is independent of queued SDL
            // callbacks and stops during pause. Wall time keeps advancing
            // then, so it must not replace this clock after Resume.
            seconds = audioSeconds;
#else
            if (audioSeconds >= wallSeconds - 0.5 && audioSeconds <= wallSeconds + 0.25) {
                seconds = audioSeconds;
            }
#endif
        }
    }

    const int target = s_baseFrame + (int)(seconds * s_fps);

    // Decode forward to the frame due now (the decode itself advances the
    // index from the frame's timestamp).
    while (s_frameIndex < target && !s_eof) {
        if (!DecodeNextVideoFrame()) break;
    }
    Present();

    // End of the segment: the requested cut point, or the last frame once the
    // clock has moved past it.
    if (s_playToFrame > 0 && s_frameIndex >= s_playToFrame) {
        s_active = FALSE;
        s_endEvent = TRUE;
        return;
    }
    if (s_eof && target > s_frameIndex) {
        s_active = FALSE;
        s_endEvent = TRUE;
    }
}

void plat_video_set_window_mode(BOOL forVideo)
{
    (void)forVideo;   // SDL has no MCI overlay to accommodate
}
