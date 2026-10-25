// video.cpp - FMV backend.
//
// Two decoders behind the one plat_video_* interface, chosen by extension:
//
//   *.avi   MCI (avivideo). The legacy Cinepak movies render into the game
//           window themselves, so plat_video_tick() does nothing and the
//           original cut points (frame numbers at the AVI's 10 fps) apply.
//   other   Media Foundation Source Reader (H.264/AAC .mp4 from
//           tools/str_to_video.py). Like the Linux ffmpeg backend, this one
//           decodes frames into a MarniDX texture and draws it as a
//           full-screen quad; the movie audio is decoded to PCM up front and
//           played through waveOut, whose play cursor is the video clock.
//
// The shared 4-state machine, the skip masks and the prologue scenario cut
// live in src/video/VideoPlayback.cpp; this is only the decoder/presenter it
// drives.
#include "../platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

#include "../../Globals.h"
#include "../../marni/MarniDX.h"
#include "../../marni/MarniSystem.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

static PlatVideoOverlayCallback s_overlayCallback = NULL;

// ===========================================================================
// MCI backend (legacy Cinepak .avi)
// ===========================================================================
static BOOL   s_windowCreated = FALSE;
static BOOL   s_savedFullScreen = FALSE;
static double s_mciFps = 10.0;   // from the AVI's own header

static unsigned int RdU32(const unsigned char* p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// The cut points arrive as times; MCI addresses frames, so read the movie's
// own rate rather than assuming the originals' 10 fps.
static double AviFrameRate(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL) return 10.0;
    unsigned char buf[16384];
    const size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    double fps = 0.0;
    for (size_t i = 0; i + 12 <= n; ++i) {
        if (memcmp(buf + i, "avih", 4) == 0) {
            const unsigned int us = RdU32(buf + i + 8);
            if (us > 0) { fps = 1e6 / (double)us; break; }
        }
    }
    if (fps <= 0.0) {
        for (size_t i = 0; i + 36 <= n; ++i) {
            if (memcmp(buf + i, "strh", 4) == 0) {
                const unsigned int scale = RdU32(buf + i + 8 + 20);
                const unsigned int rate  = RdU32(buf + i + 8 + 24);
                if (scale > 0 && rate > 0) fps = (double)rate / scale;
                break;
            }
        }
    }
    if (fps < 1.0 || fps > 120.0) fps = 10.0;
    return fps;
}

static MCIERROR MCISend(const char* cmd, BOOL showError)
{
    char buf[256];
    MCIERROR err = mciSendStringA(cmd, buf, sizeof(buf), g_hWnd);
    if (err != 0 && showError) {
        mciGetErrorStringA(err, buf, sizeof(buf));
        plat_debug_output("[VIDEO] MCI error: ");
        plat_debug_output(buf);
        plat_debug_output("\n");
    }
    return err;
}

// CloseAll - Close the MCI AVI device entirely.
// NOTE: the MCI AVI video renders directly into g_hWnd (see OpenAndPlay:
// "window movie handle <g_hWnd>"), so we must NOT send "window movie state
// hide" here - that would hide the entire game window. The stale backbuffer
// flash is handled by presenting a fresh black frame in state 3.
static void MciCloseAll(void)
{
    MCISend("close all", FALSE);
    s_windowCreated = FALSE;
    g_mciVideoDeviceID = 0;
}

static BOOL MciOpenAndPlay(const char* absPath, int playToMs)
{
    char cmd[512];
    sprintf_s(cmd, sizeof(cmd), "open \"%s\" type avivideo alias movie", absPath);

    MCIERROR err = mciSendStringA(cmd, NULL, 0, g_hWnd);
    if (err != 0) {
        char errorBuf[256];
        mciGetErrorStringA(err, errorBuf, sizeof(errorBuf));
        plat_debug_output("[VIDEO] Failed to open video: ");
        plat_debug_output(absPath);
        plat_debug_output(" - ");
        plat_debug_output(errorBuf);
        plat_debug_output("\n");
        return FALSE;
    }

    // Set the video window as a child of the game window, then show it.
    sprintf_s(cmd, sizeof(cmd), "window movie handle %u", (UINT)(UINT_PTR)g_hWnd);
    mciSendStringA(cmd, NULL, 0, g_hWnd);
    mciSendStringA("window movie state show", NULL, 0, g_hWnd);

    s_windowCreated = TRUE;

    // Fill the whole client area. The MCI AVI video is always 320x240; MCI
    // handles centering within the destination.
    RECT clientRect;
    GetClientRect(g_hWnd, &clientRect);
    sprintf_s(cmd, sizeof(cmd), "put movie destination at 0 0 %d %d",
              clientRect.right - 1, clientRect.bottom - 1);
    mciSendStringA(cmd, NULL, 0, g_hWnd);

    // The cut points are frame numbers; MCIAVI already defaults to the frames
    // time format, but pin it so a driver default cannot reinterpret them.
    mciSendStringA("set movie time format frames", NULL, 0, g_hWnd);

    const int playToFrame = (playToMs > 0)
        ? (int)((double)playToMs / 1000.0 * s_mciFps + 0.5) : 0;
    if (playToFrame > 0) {
        sprintf_s(cmd, sizeof(cmd), "play movie from 0 to %d notify",
                  playToFrame);
    } else {
        sprintf_s(cmd, sizeof(cmd), "play movie notify");
    }
    err = mciSendStringA(cmd, NULL, 0, g_hWnd);
    if (err != 0) {
        plat_debug_output("[VIDEO] MCI play failed\n");
        g_mciVideoDeviceID = 0;
        return FALSE;
    }

    g_mciVideoDeviceID = 1;
    return TRUE;
}

static void MciPlayFrom(int playFromMs)
{
    char cmd[128];
    const int playFrom = (int)((double)playFromMs / 1000.0 * s_mciFps + 0.5);
    sprintf_s(cmd, sizeof(cmd), "play movie from %d notify", playFrom);
    if (mciSendStringA(cmd, NULL, 0, g_hWnd) != 0) {
        plat_debug_output("[VIDEO] MCI resume-play failed\n");
        g_mciVideoDeviceID = 0;
        return;
    }
    g_mciVideoDeviceID = 1;
}

// ===========================================================================
// Media Foundation backend (modern containers)
// ===========================================================================
namespace mf {

static BOOL             s_ready   = FALSE;   // MFStartup succeeded
static IMFSourceReader* s_reader  = NULL;
static IMFSourceReader* s_areader = NULL;

static int    s_width  = 0;
static int    s_height = 0;
static double s_fps    = 10.0;

static unsigned char* s_rgba  = NULL;
static int            s_pitch = 0;
// Row order hint from the negotiated type. The Source Reader's RGB32 type does
// not actually carry MF_MT_DEFAULT_STRIDE (GetUINT32 returns
// MF_E_ATTRIBUTENOTFOUND), so this defaults to top-down and the per-sample
// IMF2DBuffer pitch is what decides the real order in DecodeNext.
static BOOL           s_topDown = TRUE;

static MarniHandle s_tex  = MARNI_NULL_HANDLE;
static int         s_texW = 0;
static int         s_texH = 0;

static int  s_frameIndex  = -1;
// Timestamp of the stream's first picture. Media Foundation ignores the MP4
// edit list, so an H.264 track with B-frames reports its first picture at the
// composition delay (2 frames, 0.1333 s, in the converted PS1 movies) instead
// of 0; frame numbers and seek positions are measured from here. -1 = not yet
// read.
static LONGLONG s_tsOrigin = -1;
static int  s_baseFrame   = 0;
static int  s_playToFrame = 0;
static BOOL s_active     = FALSE;
static BOOL s_endEvent   = FALSE;
static BOOL s_eof        = FALSE;
static DWORD s_startTicks = 0;

// Audio: the whole movie decoded to S16 stereo, played by waveOut. The device
// cursor is the clock (see Tick), so the picture cannot drift from the sound.
static short*  s_audio       = NULL;
static long    s_audioFrames = 0;          // frames per channel
static int     s_audioRate   = 22050;
static long    s_audioStart  = 0;          // first sample of the current segment
static long    s_audioEnd    = 0;          // one past the last sample
static BOOL    s_useAudio    = FALSE;
static HWAVEOUT s_wave       = NULL;
static WAVEHDR  s_hdr        = {};
static BOOL     s_hdrPrepared = FALSE;

static void ReleaseAudio(void)
{
    if (s_wave != NULL) {
        waveOutReset(s_wave);
        if (s_hdrPrepared) {
            waveOutUnprepareHeader(s_wave, &s_hdr, sizeof(s_hdr));
            s_hdrPrepared = FALSE;
        }
        waveOutClose(s_wave);
        s_wave = NULL;
    }
    ZeroMemory(&s_hdr, sizeof(s_hdr));
    free(s_audio);
    s_audio = NULL;
    s_audioFrames = 0;
    s_useAudio = FALSE;
}

static void ReleaseReader(void)
{
    if (s_reader != NULL) { s_reader->Release(); s_reader = NULL; }
    if (s_areader != NULL) { s_areader->Release(); s_areader = NULL; }
    free(s_rgba);
    s_rgba = NULL;
    s_pitch = 0;
    s_topDown = FALSE;

    if (s_tex != MARNI_NULL_HANDLE) {
        MarniDX* dx = Marni_DX();
        if (dx != NULL) dx->DestroyTexture(s_tex);
        s_tex = MARNI_NULL_HANDLE;
    }
    s_texW = s_texH = 0;

    ReleaseAudio();
    s_width = s_height = 0;
    s_frameIndex = -1;
    s_tsOrigin = -1;
    s_baseFrame = 0;
    s_playToFrame = 0;
    s_eof = FALSE;
    s_active = FALSE;
    s_endEvent = FALSE;
}

// --- helpers ---------------------------------------------------------------
static BOOL SetVideoType(IMFSourceReader* reader, int w, int h)
{
    IMFMediaType* type = NULL;
    if (FAILED(MFCreateMediaType(&type))) return FALSE;
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    MFSetAttributeSize(type, MF_MT_FRAME_SIZE, (UINT32)w, (UINT32)h);
    HRESULT hr = reader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
    type->Release();
    return SUCCEEDED(hr);
}

static BOOL OpenVideo(const char* path, int* outW, int* outH, double* outFps)
{
    WCHAR wpath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH) == 0) {
        if (MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH) == 0) {
            return FALSE;
        }
    }

    // Enable the advanced video processing path (the video-processor MFT that
    // converts H.264 -> RGB32) through the reader's creation attributes.
    IMFAttributes* attrs = NULL;
    if (SUCCEEDED(MFCreateAttributes(&attrs, 1))) {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    }
    HRESULT openHr = MFCreateSourceReaderFromURL(wpath, attrs, &s_reader);
    if (attrs != NULL) attrs->Release();
    if (FAILED(openHr)) return FALSE;

    // Native size/rate first, then ask for RGB32.
    IMFMediaType* native = NULL;
    UINT32 w = 0, h = 0, fpsNum = 0, fpsDen = 1;
    if (SUCCEEDED(s_reader->GetNativeMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &native))) {
        MFGetAttributeSize(native, MF_MT_FRAME_SIZE, &w, &h);
        MFGetAttributeRatio(native, MF_MT_FRAME_RATE, &fpsNum, &fpsDen);
        native->Release();
    }
    if (w == 0 || h == 0) return FALSE;

    if (!SetVideoType(s_reader, (int)w, (int)h)) return FALSE;

    s_reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    s_reader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

    // Read back the negotiated type (the processor may keep the size).
    IMFMediaType* current = NULL;
    if (SUCCEEDED(s_reader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current))) {
        UINT32 cw = 0, ch = 0;
        MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &cw, &ch);
        if (cw) w = cw;
        if (ch) h = ch;
        UINT32 stride = 0;
        if (SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
            s_topDown = ((INT32)stride > 0);
        }
        current->Release();
    }

    *outW = (int)w;
    *outH = (int)h;
    *outFps = (fpsDen != 0 && fpsNum != 0) ? (double)fpsNum / (double)fpsDen
                                          : 10.0;
    return TRUE;
}

// Grow-only audio target. Returns the buffer, or NULL when MF cannot give PCM.
static BOOL OpenAudio(const char* path)
{
    WCHAR wpath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH) == 0) {
        if (MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH) == 0) {
            return FALSE;
        }
    }

    if (FAILED(MFCreateSourceReaderFromURL(wpath, NULL, &s_areader))) {
        return FALSE;
    }

    IMFMediaType* type = NULL;
    if (FAILED(MFCreateMediaType(&type))) return FALSE;
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)s_audioRate);
    HRESULT hr = s_areader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    type->Release();
    if (FAILED(hr)) return FALSE;

    s_areader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    s_areader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    long capacity = 0;
    long written  = 0;
    for (;;) {
        DWORD flags = 0;
        IMFSample* sample = NULL;
        if (FAILED(s_areader->ReadSample(
                (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags,
                NULL, &sample))) {
            break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (sample) sample->Release();
            break;
        }
        if (sample == NULL) continue;

        IMFMediaBuffer* buf = NULL;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
            BYTE* data = NULL;
            DWORD len = 0;
            if (SUCCEEDED(buf->Lock(&data, NULL, &len))) {
                const long add = (long)len / (2 * 2);   // s16 stereo frames
                if (written + add + 1 > capacity) {
                    long want = (written + add + 1) * 2;
                    short* grown = (short*)realloc(
                        s_audio, (size_t)want * 2 * sizeof(short));
                    if (grown == NULL) { buf->Unlock(); buf->Release();
                                         sample->Release(); break; }
                    s_audio = grown;
                    capacity = want;
                }
                memcpy(s_audio + (size_t)written * 2, data,
                       (size_t)add * 2 * sizeof(short));
                written += add;
                buf->Unlock();
            }
            buf->Release();
        }
        sample->Release();
    }

    s_audioFrames = written;
    s_useAudio = (written > 0) ? TRUE : FALSE;

    if (s_useAudio) {
        WAVEFORMATEX wfx;
        ZeroMemory(&wfx, sizeof(wfx));
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = (DWORD)s_audioRate;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = 4;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * 4;
        if (waveOutOpen(&s_wave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != 0) {
            s_wave = NULL;
            s_useAudio = FALSE;
        }
    }
    return s_useAudio;
}

// Copy one decoded picture into the top-down RGBA buffer. `rowPitch` is the
// sample's own stride; scanline 0 is the top row of the image, so stepping by
// `rowPitch` walks the picture top to bottom for either sign (a negative pitch
// means the rows are stored bottom-up in memory).
static void CopyFrameRows(const BYTE* data, LONG rowPitch)
{
    for (int y = 0; y < s_height; ++y) {
        const BYTE* src = data + (ptrdiff_t)y * (ptrdiff_t)rowPitch;
        unsigned char* dst = s_rgba + (size_t)y * s_pitch;
        for (int x = 0; x < s_width; ++x) {
            dst[0] = src[2];   // RGB32 is BGRX in memory
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 255;
            src += 4;
            dst += 4;
        }
    }
}

// Decode the next picture. FALSE at end of stream.
static BOOL DecodeNext(void)
{
    if (s_eof) return FALSE;

    DWORD flags = 0;
    LONGLONG ts = 0;
    IMFSample* sample = NULL;
    HRESULT hr = s_reader->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &ts,
        &sample);
    if (FAILED(hr)) { s_eof = TRUE; return FALSE; }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        s_eof = TRUE;
        if (sample) sample->Release();
        return FALSE;
    }
    if (sample == NULL) return FALSE;

    IMFMediaBuffer* buf = NULL;
    if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
        if (s_rgba == NULL) {
            s_pitch = s_width * 4;
            s_rgba = (unsigned char*)malloc((size_t)s_pitch * s_height);
        }
        if (s_rgba != NULL) {
            // Take the row order from the sample, not the media type. The
            // Source Reader's RGB32 type has no MF_MT_DEFAULT_STRIDE, so
            // assuming bottom-up mirrored the whole movie. Lock2D reports the
            // real stride, and its scanline 0 is the top row of the image
            // whether the rows run up or down in memory.
            IMF2DBuffer* buf2d = NULL;
            BYTE* data = NULL;
            LONG rowPitch = 0;
            if (SUCCEEDED(buf->QueryInterface(__uuidof(IMF2DBuffer),
                                              (void**)&buf2d))) {
                if (SUCCEEDED(buf2d->Lock2D(&data, &rowPitch)) && rowPitch != 0) {
                    CopyFrameRows(data, rowPitch);
                    buf2d->Unlock2D();
                }
                buf2d->Release();
            } else {
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&data, NULL, &len))) {
                    // No 2D interface: fall back to the negotiated type's
                    // stride, or the packed width when it names none.
                    rowPitch = s_topDown ? (LONG)s_width * 4
                                         : -(LONG)s_width * 4;
                    CopyFrameRows(data, rowPitch);
                    buf->Unlock();
                }
            }
        }
        buf->Release();
    }
    sample->Release();

    // Open decodes the first picture before anything seeks, so the first
    // timestamp read is the stream's frame 0.
    if (s_tsOrigin < 0) s_tsOrigin = ts > 0 ? ts : 0;
    if (ts >= s_tsOrigin) {
        s_frameIndex = (int)((double)(ts - s_tsOrigin) / 1e7 * s_fps + 0.5);
    } else {
        s_frameIndex++;
    }
    return TRUE;
}

static void Present(void)
{
    if (s_rgba == NULL || s_width <= 0 || s_height <= 0) return;

    MarniDX* dx = Marni_DX();
    if (dx == NULL) return;

    if (s_tex == MARNI_NULL_HANDLE || s_texW != s_width || s_texH != s_height) {
        if (s_tex != MARNI_NULL_HANDLE) dx->DestroyTexture(s_tex);
        int ow = 0, oh = 0;
        s_tex = dx->CreateTexture(s_width, s_height, 32, s_rgba, &ow, &oh);
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
                       0xFFFFFFFFu, s_tex, MARNI_SAMPLER_POINT,
                       MARNI_BLEND_DISABLE);
    }
    dx->Present();
}

static void StartAudioSegment(void)
{
    if (!s_useAudio || s_wave == NULL) return;
    if (s_hdrPrepared) {
        waveOutReset(s_wave);
        waveOutUnprepareHeader(s_wave, &s_hdr, sizeof(s_hdr));
        s_hdrPrepared = FALSE;
    }
    ZeroMemory(&s_hdr, sizeof(s_hdr));

    long start = s_audioStart;
    long end   = (s_audioEnd > start) ? s_audioEnd : s_audioFrames;
    if (start < 0) start = 0;
    if (end > s_audioFrames) end = s_audioFrames;
    if (end <= start) return;

    s_hdr.lpData = (LPSTR)(s_audio + (size_t)start * 2);
    s_hdr.dwBufferLength = (DWORD)((end - start) * 2 * sizeof(short));
    if (waveOutPrepareHeader(s_wave, &s_hdr, sizeof(s_hdr)) != MMSYSERR_NOERROR) {
        return;
    }
    s_hdrPrepared = TRUE;
    waveOutWrite(s_wave, &s_hdr, sizeof(s_hdr));
}

// Position the decoder (and the audio cursor) at `frame`.
static BOOL SeekTo(int frame)
{
    if (frame < 0) frame = 0;

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = (LONGLONG)((double)frame / s_fps * 1e7) +
                        (s_tsOrigin > 0 ? s_tsOrigin : 0);
    s_reader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    s_eof = FALSE;
    s_frameIndex = -1;
    while (s_frameIndex < frame && !s_eof) {
        if (!DecodeNext()) break;
    }

    if (s_useAudio) {
        s_audioStart = (long)((double)frame / s_fps * s_audioRate);
        s_audioEnd = (s_playToFrame > 0)
            ? (long)((double)s_playToFrame / s_fps * s_audioRate)
            : s_audioFrames;
        StartAudioSegment();
    }

    s_baseFrame = frame;
    s_startTicks = plat_time_ms();
    return TRUE;
}

// ===========================================================================
// Entry points
// ===========================================================================
BOOL Init(void)
{
    if (s_ready) return TRUE;
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        plat_debug_output("[VIDEO] MFStartup failed\n");
        return FALSE;
    }
    s_ready = TRUE;
    return TRUE;
}

BOOL Open(const char* path, int playToMs)
{
    if (!Init()) return FALSE;
    ReleaseReader();

    if (!OpenVideo(path, &s_width, &s_height, &s_fps)) {
        plat_debug_output("[VIDEO] Media Foundation could not open video\n");
        ReleaseReader();
        return FALSE;
    }

    OpenAudio(path);   // optional; picture still plays when it fails

    // Times to frames with this stream's rate (a converted movie need not be
    // the originals' 10 fps).
    s_playToFrame = (playToMs > 0)
        ? (int)((double)playToMs / 1000.0 * s_fps + 0.5) : 0;
    s_active = TRUE;
    s_endEvent = FALSE;
    SeekTo(0);
    Present();
    return TRUE;
}

void PlayFrom(int fromMs)
{
    if (s_reader == NULL) return;
    const int fromFrame = (int)((double)fromMs / 1000.0 * s_fps + 0.5);
    s_playToFrame = 0;
    s_endEvent = FALSE;
    s_active = TRUE;
    SeekTo(fromFrame);
    Present();
}

void Stop(void)
{
    s_active = FALSE;
    ReleaseAudio();
}

void Close(void)
{
    ReleaseReader();
}

BOOL IsActive(void)
{
    return s_active;
}

// TRUE while this backend owns the movie (open or just finished), so the
// dispatch functions know not to fall through to MCI.
BOOL HasMedia(void)
{
    return (s_reader != NULL) || s_active;
}

BOOL TakeEndEvent(void)
{
    if (!s_endEvent) return FALSE;
    s_endEvent = FALSE;
    return TRUE;
}

void Tick(void)
{
    if (!s_active || s_reader == NULL) return;

    // Audio cursor is the clock where it exists and is trustworthy; the wall
    // clock is the fallback for silent movies or a device that discards output.
    const double wallSeconds =
        (double)(plat_time_ms() - s_startTicks) / 1000.0;
    double seconds = wallSeconds;
    if (s_useAudio && s_wave != NULL) {
        MMTIME mmt;
        ZeroMemory(&mmt, sizeof(mmt));
        mmt.wType = TIME_SAMPLES;
        if (waveOutGetPosition(s_wave, &mmt, sizeof(mmt)) == MMSYSERR_NOERROR &&
            mmt.wType == TIME_SAMPLES) {
            const double audioSeconds =
                (double)((long)mmt.u.sample - s_audioStart) / s_audioRate;
            if (audioSeconds >= wallSeconds - 0.5 &&
                audioSeconds <= wallSeconds + 0.25) {
                seconds = audioSeconds;
            }
        }
    }

    const int target = s_baseFrame + (int)(seconds * s_fps);
    while (s_frameIndex < target && !s_eof) {
        if (!DecodeNext()) break;
    }
    Present();

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

}  // namespace mf

// ===========================================================================
// Dispatch (see platform.h)
// ===========================================================================
static BOOL IsLegacyAvi(const char* path)
{
    const char* dot = strrchr(path, '.');
    return dot != NULL && _stricmp(dot, ".avi") == 0;
}

void plat_video_set_overlay_callback(PlatVideoOverlayCallback callback)
{
    s_overlayCallback = callback;
}

BOOL plat_video_init(void)
{
    s_windowCreated = FALSE;
    g_mciVideoDeviceID = 0;

    // MCI is only needed for the legacy AVIs; its driver may be missing on a
    // minimal system, which must not stop the modern backend. Either backend
    // being usable is enough to report FMV support.
    MCIERROR err = mciSendStringA("open avivideo alias _test_", NULL, 0, NULL);
    BOOL mciOk = FALSE;
    if (err == 0) {
        mciSendStringA("close _test_", NULL, 0, NULL);
        mciOk = TRUE;
    } else {
        plat_debug_output("[VIDEO] OpenMCIAviVideo failed - MCI AVI driver "
                          "not available\n");
    }

    return (mciOk || mf::Init()) ? TRUE : FALSE;
}

BOOL plat_video_open_and_play(const char* filePath, int playToMs)
{
    mf::Close();
    MciCloseAll();

    // Convert to absolute path (MCI and MF both want one).
    char absPath[MAX_PATH];
    if (GetFullPathNameA(filePath, sizeof(absPath), absPath, NULL) == 0) {
        strcpy_s(absPath, sizeof(absPath), filePath);
    }

    if (GetFileAttributesA(absPath) == INVALID_FILE_ATTRIBUTES) {
        plat_debug_output("[VIDEO] Video file not found: ");
        plat_debug_output(absPath);
        plat_debug_output("\n");
        return FALSE;
    }

    if (IsLegacyAvi(absPath)) {
        s_mciFps = AviFrameRate(absPath);
        return MciOpenAndPlay(absPath, playToMs);
    }
    return mf::Open(absPath, playToMs);
}

void plat_video_play_from(int fromMs)
{
    if (mf::IsActive() || mf::HasMedia()) {
        mf::PlayFrom(fromMs);
    } else {
        MciPlayFrom(fromMs);
    }
}

void plat_video_stop(void)
{
    if (mf::HasMedia()) {
        mf::Stop();
    } else {
        MCISend("stop movie", FALSE);
        g_mciVideoDeviceID = 0;
    }
}

void plat_video_close(void)
{
    mf::Close();
    MciCloseAll();
}

BOOL plat_video_is_active(void)
{
    if (mf::HasMedia()) return mf::IsActive();
    return (g_mciVideoDeviceID != 0);
}

BOOL plat_video_take_end_event(void)
{
    if (mf::HasMedia()) {
        if (mf::TakeEndEvent()) return TRUE;
        return FALSE;
    }
    if (!g_bMCIVideoEvent) return FALSE;
    g_bMCIVideoEvent = FALSE;
    return TRUE;
}

void plat_video_tick(void)
{
    mf::Tick();   // MCI paints the window itself
}

// ============================================================================
// SetWindowMode - the original widens/borders the window around the movie for
// the two adapter ids that use a GDI overlay (5 and 7), then restores it. MCI
// renders into the window, so the style matters there; the Media Foundation
// path presents through MarniDX, so it does not.
// ============================================================================
void plat_video_set_window_mode(BOOL forVideo)
{
    if (g_dwSelectedDisplayAdapterID != 5 && g_dwSelectedDisplayAdapterID != 7) {
        return;
    }
    if (forVideo) {
        s_savedFullScreen = g_bFullScreen;
        LONG_PTR style = GetWindowLongA(g_hWnd, GWL_STYLE);
        if (!g_bFullScreen) {
            style = (style & 0xFFFAFFFF) | 0xC00000;
        } else {
            style = style & 0xFF3AFFFF;
        }
        SetWindowLongA(g_hWnd, GWL_STYLE, style);
    } else {
        LONG_PTR style = GetWindowLongA(g_hWnd, GWL_STYLE);
        SetWindowLongA(g_hWnd, GWL_STYLE, style & 0xFFFAFFFF | 0xC00000);
    }
}
