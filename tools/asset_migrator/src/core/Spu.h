#pragma once
// PS1 sound decoding: SPU-ADPCM (VAG) and CD-XA, the VAB bank layout, and an
// SPU + libsd sequencer model that renders a SEPxx.HSB BGM track to PCM.
//
// Hardware behaviour (ADPCM filters, the 4-point "gaussian" interpolation
// table, the ADSR envelope stepping and the reverb unit) follows the psx-spx
// SPU reference. Everything libsd decides - how a key-on turns velocity,
// channel, tone and program volume and pan into the voice registers, the
// loop markers, the reverb preset - was read out of SLUS_005.51 and is noted
// where it is used (Spu.cpp).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace re1 {
namespace spu {

// Decode an SPU-ADPCM stream up to its first END flag, ignoring the loop
// (one pass, the way the PC renders its SFX). `loopStart` receives the sample
// index of the last LOOP-START block before the end, or -1, and `repeats`
// whether the end block jumps back there.
std::vector<int16_t> decodeVag(const uint8_t* data, size_t size,
                               long* loopStart = nullptr,
                               bool* repeats = nullptr);

// Decode mono 4-bit CD-XA: each raw 2352-byte sector's 18 sound groups.
std::vector<int16_t> decodeXaMono(const std::vector<const uint8_t*>& sectors);

// One tone of a VAB program (VagAtr / ToneAtr, 32 bytes).
struct Tone {
    int prog = 0;
    uint8_t mode = 0;       // bit 2 = reverb
    uint8_t vol = 0;
    uint8_t pan = 64;
    uint8_t centre = 60;
    uint8_t fine = 0;       // libsd "shift", in 1/128 semitone
    uint8_t minNote = 0;
    uint8_t maxNote = 127;
    uint8_t pbMin = 0;
    uint8_t pbMax = 0;
    uint16_t adsr1 = 0;
    uint16_t adsr2 = 0;
    uint16_t vag = 0;       // 1-based VAG index
};

struct Program {
    uint8_t vol = 127;
    uint8_t pan = 64;
    std::vector<Tone> tones;
};

// A VAB: header (`pBAV`) plus body. Tone block k belongs to the k-th program
// that has any tones (checked against every ToneAtr's own program field over
// all 57 BGM banks: 901/901).
class Vab {
public:
    bool parse(const uint8_t* hdr, size_t hdrSize, const uint8_t* body,
               size_t bodySize, std::string* error);

    const Program* program(int prog) const;
    // The VAG bytes of 1-based index `vag` (empty span when out of range).
    const uint8_t* vagData(int vag, size_t* size) const;

private:
    std::vector<Program> m_progs;   // 128, most empty
    std::vector<size_t> m_vagOff;   // 256
    std::vector<size_t> m_vagSize;  // 256
    const uint8_t* m_body = nullptr;
    size_t m_bodySize = 0;
};

// The rate the SPU plays a tone at when keyed at `note`:
// 44100 * 2^((note - centre + fine/128) / 12). The +fine sign is the one that
// correlates with the PC's pre-rendered files (the other never wins).
double toneRate(const Tone& t, int note);

// libsd key-on volume, as SpuVmKeyOnNow (SLUS_005.51 0x80082a48) computes it
// with velocity and channel volume at 127: the linear product scaled to
// 0x3FFF is SQUARED into the register. Returns the amplitude (0..1).
double toneGain(const Tone& t, const Program& p);

// A rendered BGM track: interleaved stereo at `rate`, at the SPU's own
// scale but NOT clamped, so the caller can level it before converting to
// 16-bit. `loopStart`/`loopEnd` are sample frames ([start, end)) when the
// sequence loops, else -1.
struct RenderedTrack {
    std::vector<int32_t> mix;
    int rate = 44100;
    long loopStart = -1;
    long loopEnd = -1;
    int notes = 0;
};

// Render track `track` of a SEPxx.HSB (VAB + appended SEP) one pass long.
// A looping track is rendered intro + the loop's SECOND pass, so the loop
// region already carries the tails of its own previous pass and repeats
// seamlessly.
bool renderSepTrack(const uint8_t* hsb, size_t size, int track,
                    RenderedTrack* out, std::string* error);

}  // namespace spu
}  // namespace re1
