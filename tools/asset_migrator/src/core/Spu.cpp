#include "core/Spu.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace re1 {
namespace spu {
namespace {

// ADPCM prediction filters (SPU and CD-XA share them).
const int kFilter[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

// The SPU's 4-point interpolation table (psx-spx "4-Point Gaussian
// Interpolation"). Every tap group gauss[i] + gauss[0FFh-i] + gauss[100h+i] +
// gauss[1FFh-i] sums to 7F7Fh..7F81h, which is how this copy was checked.
const int16_t kGauss[512] = {
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 2, 2, 2, 3, 3,
    3, 4, 4, 5, 5, 6, 7, 7,
    8, 9, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 21, 22, 24,
    25, 27, 28, 30, 32, 33, 35, 37,
    39, 41, 44, 46, 48, 51, 53, 56,
    58, 61, 64, 67, 70, 73, 77, 80,
    84, 87, 91, 95, 99, 103, 107, 111,
    116, 120, 125, 130, 135, 140, 145, 150,
    156, 161, 167, 173, 179, 186, 192, 199,
    205, 212, 219, 227, 234, 242, 250, 257,
    266, 274, 283, 291, 300, 309, 319, 328,
    338, 348, 358, 369, 379, 390, 401, 412,
    424, 436, 448, 460, 473, 485, 498, 512,
    525, 539, 553, 567, 582, 597, 612, 627,
    643, 659, 675, 692, 708, 726, 743, 761,
    779, 797, 816, 835, 854, 874, 894, 914,
    935, 956, 977, 999, 1020, 1043, 1066, 1089,
    1112, 1136, 1160, 1184, 1209, 1234, 1260, 1286,
    1312, 1339, 1366, 1394, 1422, 1450, 1479, 1508,
    1537, 1567, 1598, 1628, 1660, 1691, 1723, 1756,
    1789, 1822, 1856, 1890, 1924, 1959, 1995, 2031,
    2067, 2104, 2141, 2179, 2217, 2256, 2295, 2334,
    2374, 2415, 2456, 2497, 2539, 2582, 2624, 2668,
    2712, 2756, 2801, 2846, 2892, 2938, 2985, 3032,
    3079, 3128, 3176, 3225, 3275, 3325, 3376, 3427,
    3479, 3531, 3584, 3637, 3691, 3745, 3799, 3855,
    3910, 3967, 4023, 4081, 4138, 4197, 4255, 4315,
    4374, 4435, 4495, 4557, 4619, 4681, 4744, 4807,
    4871, 4935, 5000, 5065, 5131, 5197, 5264, 5332,
    5399, 5468, 5536, 5606, 5676, 5746, 5817, 5888,
    5959, 6032, 6104, 6177, 6251, 6325, 6400, 6475,
    6550, 6626, 6702, 6779, 6856, 6934, 7012, 7091,
    7170, 7249, 7329, 7409, 7490, 7571, 7653, 7735,
    7817, 7900, 7983, 8066, 8150, 8234, 8319, 8404,
    8489, 8575, 8661, 8748, 8834, 8922, 9009, 9097,
    9185, 9273, 9362, 9451, 9541, 9630, 9720, 9811,
    9901, 9992, 10083, 10174, 10266, 10358, 10450, 10542,
    10635, 10727, 10820, 10913, 11007, 11100, 11194, 11288,
    11382, 11476, 11571, 11665, 11760, 11855, 11950, 12045,
    12140, 12236, 12331, 12427, 12522, 12618, 12714, 12809,
    12905, 13001, 13097, 13193, 13289, 13385, 13481, 13577,
    13673, 13769, 13865, 13961, 14056, 14152, 14248, 14343,
    14439, 14534, 14630, 14725, 14820, 14915, 15010, 15104,
    15199, 15293, 15387, 15481, 15575, 15669, 15762, 15855,
    15948, 16041, 16133, 16226, 16317, 16409, 16500, 16592,
    16682, 16773, 16863, 16953, 17042, 17131, 17220, 17308,
    17396, 17484, 17571, 17658, 17744, 17830, 17916, 18001,
    18086, 18170, 18254, 18337, 18420, 18502, 18584, 18665,
    18746, 18826, 18905, 18985, 19063, 19141, 19219, 19295,
    19372, 19447, 19522, 19597, 19671, 19744, 19816, 19888,
    19959, 20030, 20100, 20169, 20238, 20306, 20373, 20439,
    20505, 20570, 20634, 20698, 20760, 20822, 20884, 20944,
    21004, 21063, 21121, 21178, 21235, 21290, 21345, 21399,
    21452, 21505, 21556, 21607, 21657, 21706, 21754, 21801,
    21848, 21893, 21938, 21982, 22025, 22066, 22107, 22148,
    22187, 22225, 22262, 22299, 22334, 22369, 22402, 22435,
    22467, 22498, 22527, 22556, 22584, 22611, 22637, 22662,
    22686, 22709, 22731, 22752, 22772, 22791, 22809, 22826,
    22842, 22857, 22872, 22885, 22897, 22908, 22918, 22927,
    22935, 22942, 22948, 22953, 22957, 22960, 22962, 22963,
};

// Reverb buffer resampling FIR (psx-spx "Reverb Buffer Resampling").
const int16_t kReverbFir[39] = {
    -0x0001, 0, 0x0002, 0, -0x000A, 0, 0x0023, 0, -0x0067, 0, 0x010A, 0,
    -0x0268, 0, 0x0534, 0, -0x0B90, 0, 0x2806, 0x4000, 0x2806, 0, -0x0B90, 0,
    0x0534, 0, -0x0268, 0, 0x010A, 0, -0x0067, 0, 0x0023, 0, -0x000A, 0,
    0x0002, 0, -0x0001};

inline int clamp16(int v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v); }
inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

// One ADPCM nibble -> sample. Shift values 13..15 behave like 9 (psx-spx).
inline int adpcm(int nibble, int shift, int k0, int k1, int& h1, int& h2) {
    const int n = nibble > 7 ? nibble - 16 : nibble;
    int s = (n << 12) >> shift;
    s += (h1 * k0 + h2 * k1 + 32) >> 6;
    s = clamp16(s);
    h2 = h1;
    h1 = s;
    return s;
}

void decodeBlock(const uint8_t* b, int16_t out[28], int& h1, int& h2) {
    int shift = b[0] & 15;
    if (shift > 12) shift = 9;
    int f = b[0] >> 4;
    if (f > 4) f = 4;
    const int k0 = kFilter[f][0], k1 = kFilter[f][1];
    for (int i = 0; i < 14; ++i) {
        out[i * 2] = (int16_t)adpcm(b[2 + i] & 15, shift, k0, k1, h1, h2);
        out[i * 2 + 1] = (int16_t)adpcm(b[2 + i] >> 4, shift, k0, k1, h1, h2);
    }
}

}  // namespace

std::vector<int16_t> decodeVag(const uint8_t* data, size_t size, long* loopStart,
                               bool* repeats) {
    std::vector<int16_t> out;
    int h1 = 0, h2 = 0;
    long loop = -1;
    bool rep = false;
    for (size_t p = 0; p + 16 <= size; p += 16) {
        const uint8_t flags = data[p + 1];
        if (flags & 4) loop = (long)out.size();
        int16_t s[28];
        decodeBlock(data + p, s, h1, h2);
        out.insert(out.end(), s, s + 28);
        if (flags & 1) {
            rep = (flags & 2) != 0;
            break;
        }
    }
    if (loopStart) *loopStart = loop;
    if (repeats) *repeats = rep;
    return out;
}

std::vector<int16_t> decodeXaMono(const std::vector<const uint8_t*>& sectors) {
    std::vector<int16_t> out;
    out.reserve(sectors.size() * 18 * 8 * 28);
    int h1 = 0, h2 = 0;
    for (const uint8_t* sec : sectors) {
        const uint8_t* d = sec + 24;
        for (int g = 0; g < 18; ++g) {
            const uint8_t* grp = d + g * 128;
            for (int u = 0; u < 8; ++u) {
                const int prm = grp[4 + u];
                int shift = prm & 15;
                if (shift > 12) shift = 9;
                int f = prm >> 4;
                if (f > 4) f = 4;
                for (int j = 0; j < 28; ++j) {
                    const uint8_t b = grp[16 + (u >> 1) + j * 4];
                    const int nib = (u & 1) ? (b >> 4) : (b & 15);
                    out.push_back((int16_t)adpcm(nib, shift, kFilter[f][0],
                                                 kFilter[f][1], h1, h2));
                }
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// VAB

bool Vab::parse(const uint8_t* hdr, size_t hdrSize, const uint8_t* body,
                size_t bodySize, std::string* error) {
    if (hdrSize < 0x820 || std::memcmp(hdr, "pBAV", 4) != 0) {
        if (error) *error = "not a VAB header";
        return false;
    }
    const int ps = rd16(hdr + 0x12);
    const size_t sizeTbl = 0x820 + (size_t)ps * 0x200;
    if (sizeTbl + 512 > hdrSize) {
        if (error) *error = "truncated VAB header";
        return false;
    }
    m_progs.assign(128, Program());
    int block = 0;
    for (int i = 0; i < 128; ++i) {
        const uint8_t* pa = hdr + 0x20 + i * 16;
        const int tones = pa[0];
        m_progs[i].vol = pa[1];
        m_progs[i].pan = pa[4];
        if (tones == 0) continue;
        if (block >= ps) break;
        for (int t = 0; t < tones && t < 16; ++t) {
            const uint8_t* ta = hdr + 0x820 + block * 0x200 + t * 0x20;
            Tone tn;
            tn.prog = i;
            tn.mode = ta[1];
            tn.vol = ta[2];
            tn.pan = ta[3];
            tn.centre = ta[4];
            tn.fine = ta[5];
            tn.minNote = ta[6];
            tn.maxNote = ta[7];
            tn.pbMin = ta[12];
            tn.pbMax = ta[13];
            tn.adsr1 = rd16(ta + 16);
            tn.adsr2 = rd16(ta + 18);
            tn.vag = rd16(ta + 22);
            m_progs[i].tones.push_back(tn);
        }
        ++block;
    }
    m_vagOff.assign(256, 0);
    m_vagSize.assign(256, 0);
    size_t acc = 0;
    for (int i = 1; i < 256; ++i) {
        m_vagOff[i] = acc;
        m_vagSize[i] = (size_t)rd16(hdr + sizeTbl + i * 2) * 8;
        acc += m_vagSize[i];
    }
    m_body = body;
    m_bodySize = bodySize;
    return true;
}

const Program* Vab::program(int prog) const {
    if (prog < 0 || prog >= (int)m_progs.size() || m_progs[prog].tones.empty())
        return nullptr;
    return &m_progs[prog];
}

const uint8_t* Vab::vagData(int vag, size_t* size) const {
    *size = 0;
    if (vag <= 0 || vag >= 256 || m_vagSize[vag] == 0) return nullptr;
    if (m_vagOff[vag] + m_vagSize[vag] > m_bodySize) return nullptr;
    *size = m_vagSize[vag];
    return m_body + m_vagOff[vag];
}

double toneRate(const Tone& t, int note) {
    return 44100.0 *
           std::pow(2.0, (note - t.centre + t.fine / 128.0) / 12.0);
}

double toneGain(const Tone& t, const Program& p) {
    const double v = (t.vol / 127.0) * (p.vol / 127.0);
    return v * v;
}

// ---------------------------------------------------------------------------
// SPU voice + libsd sequencer

namespace {

enum class Phase { Off, Attack, Decay, Sustain, Release };

struct EnvParams {
    bool exp = false;
    bool dec = false;
    int shift = 0;
    int step = 0;
    bool never = false;  // all-ones rate: the level never steps
};

// psx-spx "Envelope Operation depending on Shift/Step/Mode/Direction".
void envTick(const EnvParams& e, int& level, int& counter) {
    if (e.never) return;
    int s = 7 - e.step;
    if (e.dec) s = ~s;
    s <<= std::max(0, 11 - e.shift);
    int inc = 0x8000 >> std::max(0, e.shift - 11);
    if (e.exp && !e.dec && level > 0x6000) {
        if (e.shift < 10) {
            s >>= 2;
        } else if (e.shift >= 11) {
            inc >>= 2;
        } else {
            s >>= 1;
            inc >>= 1;
        }
    } else if (e.exp && e.dec) {
        s = (s * level) >> 15;
    }
    inc = std::max(inc, 1);
    counter += inc;
    if (!(counter & 0x8000)) return;
    counter = 0;
    level += s;
    if (!e.dec)
        level = std::min(std::max(level, -0x8000), 0x7FFF);
    else
        level = std::max(level, 0);
}

struct Voice {
    Phase phase = Phase::Off;
    int chan = 0;
    int note = 0;
    const Tone* tone = nullptr;
    const Program* prog = nullptr;
    const uint8_t* vag = nullptr;
    size_t vagSize = 0;
    size_t block = 0;       // byte offset of the block being played
    size_t loopBlock = 0;
    int h1 = 0, h2 = 0;
    int16_t buf[28] = {};
    int bufPos = 0;
    int hist[4] = {};       // oldest .. new
    uint32_t counter = 0;
    int pitch = 0x1000;
    int level = 0;
    int envCounter = 0;
    int volL = 0, volR = 0; // register values (0..0x3FFF)
    bool reverb = false;
    int vel = 127;
    uint64_t age = 0;
};

// libsd channel state.
struct Channel {
    int prog = 0;
    int vol = 127;
    int expr = 127;
    int pan = 64;
    int bend = 8192;
};

class Synth {
public:
    Synth(const Vab& vab, int seqVol) : m_vab(vab), m_seqVol(seqVol) {
        m_rev.assign(kRevWords, 0);
    }

    void noteOn(int ch, int note, int vel) {
        const Program* p = m_vab.program(m_chan[ch].prog);
        if (!p) return;
        // SpuVmKeyOn (0x80085720): a new key-on first keys off any voice
        // still holding this channel/note.
        noteOff(ch, note);
        for (const Tone& t : p->tones) {
            if (note < t.minNote || note > t.maxNote) continue;
            size_t n = 0;
            const uint8_t* data = m_vab.vagData(t.vag, &n);
            if (!data) continue;
            Voice& v = allocVoice();
            v = Voice();
            v.phase = Phase::Attack;
            v.chan = ch;
            v.note = note;
            v.tone = &t;
            v.prog = p;
            v.vag = data;
            v.vagSize = n;
            v.reverb = (t.mode & 4) != 0;
            v.age = ++m_age;
            v.vel = vel;
            startBlock(v);
            applyVolume(v);
            applyPitch(v);
            ++m_notes;
        }
    }

    void noteOff(int ch, int note) {
        for (Voice& v : m_voices)
            if (v.phase != Phase::Off && v.phase != Phase::Release &&
                v.chan == ch && v.note == note)
                v.phase = Phase::Release;
    }

    void allOff() {
        for (Voice& v : m_voices)
            if (v.phase != Phase::Off) v.phase = Phase::Release;
    }

    void control(int ch, int cc, int val) {
        Channel& c = m_chan[ch];
        if (cc == 7) c.vol = val;
        else if (cc == 11) c.expr = val;
        else if (cc == 10) c.pan = val;
        else return;
        for (Voice& v : m_voices)
            if (v.phase != Phase::Off && v.chan == ch) applyVolume(v);
    }

    void program(int ch, int prog) { m_chan[ch].prog = prog; }

    void bend(int ch, int value14) {
        m_chan[ch].bend = value14;
        for (Voice& v : m_voices)
            if (v.phase != Phase::Off && v.chan == ch) applyPitch(v);
    }

    bool silent() const {
        for (const Voice& v : m_voices)
            if (v.phase != Phase::Off) return false;
        return true;
    }

    int notes() const { return m_notes; }

    // One 44.1 kHz output frame.
    void render(int& outL, int& outR) {
        int dryL = 0, dryR = 0, wetL = 0, wetR = 0;
        for (Voice& v : m_voices) {
            if (v.phase == Phase::Off) continue;
            const int i = (v.counter >> 4) & 0xFF;
            int s = (kGauss[0xFF - i] * v.hist[0]) >> 15;
            s += (kGauss[0x1FF - i] * v.hist[1]) >> 15;
            s += (kGauss[0x100 + i] * v.hist[2]) >> 15;
            s += (kGauss[i] * v.hist[3]) >> 15;
            s = (s * v.level) >> 15;
            const int l = (s * (v.volL * 2)) >> 15;
            const int r = (s * (v.volR * 2)) >> 15;
            dryL += l;
            dryR += r;
            if (v.reverb) {
                wetL += l;
                wetR += r;
            }
            stepEnvelope(v);
            v.counter += (uint32_t)v.pitch;
            while (v.counter >= 0x1000 && v.phase != Phase::Off) {
                v.counter -= 0x1000;
                nextSample(v);
            }
        }
        int rl = 0, rr = 0;
        reverb(clamp16(wetL), clamp16(wetR), rl, rr);
        outL = dryL + rl;
        outR = dryR + rr;
    }

private:
    static constexpr int kVoices = 24;
    // Room preset work area (SLUS_005.51 0x800add98[1] = 0xFB28 -> 0x7D940,
    // i.e. 0x26C0 bytes up to the end of SPU RAM), in 16-bit words.
    static constexpr int kRevWords = 0x26C0 / 2;

    Voice& allocVoice() {
        for (Voice& v : m_voices)
            if (v.phase == Phase::Off) return v;
        // All 24 busy: take the quietest released voice, else the oldest.
        Voice* best = nullptr;
        for (Voice& v : m_voices)
            if (v.phase == Phase::Release && (!best || v.level < best->level))
                best = &v;
        if (best) return *best;
        best = &m_voices[0];
        for (Voice& v : m_voices)
            if (v.age < best->age) best = &v;
        return *best;
    }

    void startBlock(Voice& v) {
        v.block = 0;
        v.loopBlock = 0;
        v.h1 = v.h2 = 0;
        decodeCurrent(v);
        v.bufPos = 0;
        v.hist[0] = v.hist[1] = v.hist[2] = 0;
        v.hist[3] = v.buf[0];
    }

    void decodeCurrent(Voice& v) {
        if (v.block + 16 > v.vagSize) {
            std::memset(v.buf, 0, sizeof(v.buf));
            return;
        }
        if (v.vag[v.block + 1] & 4) v.loopBlock = v.block;
        decodeBlock(v.vag + v.block, v.buf, v.h1, v.h2);
    }

    void nextSample(Voice& v) {
        if (++v.bufPos >= 28) {
            v.bufPos = 0;
            const uint8_t flags = v.block + 16 <= v.vagSize ? v.vag[v.block + 1] : 1;
            if (flags & 1) {
                if (flags & 2) {
                    v.block = v.loopBlock;
                } else {
                    // Loop End without Repeat: forced release, level zero.
                    v.phase = Phase::Off;
                    v.level = 0;
                    return;
                }
            } else {
                v.block += 16;
            }
            decodeCurrent(v);
        }
        v.hist[0] = v.hist[1];
        v.hist[1] = v.hist[2];
        v.hist[2] = v.hist[3];
        v.hist[3] = v.buf[v.bufPos];
    }

    void stepEnvelope(Voice& v) {
        const uint16_t a1 = v.tone->adsr1, a2 = v.tone->adsr2;
        EnvParams e;
        switch (v.phase) {
        case Phase::Attack:
            e.exp = (a1 >> 15) & 1;
            e.shift = (a1 >> 10) & 0x1F;
            e.step = (a1 >> 8) & 3;
            e.never = ((e.shift << 2) | e.step) == 0x7F;
            envTick(e, v.level, v.envCounter);
            if (v.level >= 0x7FFF) {
                v.phase = Phase::Decay;
                v.envCounter = 0;
            }
            break;
        case Phase::Decay: {
            e.exp = true;
            e.dec = true;
            e.shift = (a1 >> 4) & 0xF;
            envTick(e, v.level, v.envCounter);
            const int sustain = std::min(((a1 & 0xF) + 1) * 0x800, 0x7FFF);
            if (v.level <= sustain) {
                v.phase = Phase::Sustain;
                v.envCounter = 0;
            }
            break;
        }
        case Phase::Sustain:
            e.exp = (a2 >> 15) & 1;
            e.dec = (a2 >> 14) & 1;
            e.shift = (a2 >> 8) & 0x1F;
            e.step = (a2 >> 6) & 3;
            e.never = ((e.shift << 2) | e.step) == 0x7F;
            envTick(e, v.level, v.envCounter);
            break;
        case Phase::Release:
            e.exp = (a2 >> 5) & 1;
            e.dec = true;
            e.shift = a2 & 0x1F;
            envTick(e, v.level, v.envCounter);
            if (v.level <= 0) v.phase = Phase::Off;
            break;
        case Phase::Off:
            break;
        }
    }

    // SpuVmKeyOnNow (SLUS_005.51 0x80082a48): velocity, channel volume, tone
    // volume and program volume multiply into a 0..0x3FFF linear value, the
    // SEP track volume scales it, three pan stages (tone, program, channel)
    // each attenuate one side by p/63 or (127-p)/63, and the register is the
    // SQUARE of the result over 0x3FFF.
    void applyVolume(Voice& v) {
        const Channel& c = m_chan[v.chan];
        const unsigned chVol = (unsigned)c.vol * (unsigned)c.expr / 127u;
        unsigned lin = ((unsigned)v.vel * chVol * 0x3FFFu) / 0x3F01u;
        lin = lin * v.tone->vol * v.prog->vol / 0x3F01u;
        unsigned l = lin * (unsigned)m_seqVol / 127u;
        unsigned r = l;
        for (int p : {(int)v.tone->pan, (int)v.prog->pan, c.pan}) {
            if (p < 0x40)
                r = r * (unsigned)p / 0x3Fu;
            else
                l = l * (unsigned)(0x7F - p) / 0x3Fu;
        }
        v.volL = (int)std::min(l * l / 0x3FFFu, 0x3FFFu);
        v.volR = (int)std::min(r * r / 0x3FFFu, 0x3FFFu);
    }

    void applyPitch(Voice& v) {
        const Channel& c = m_chan[v.chan];
        double semis = v.note - v.tone->centre + v.tone->fine / 128.0;
        const int b = c.bend - 8192;
        if (b > 0) semis += b / 8192.0 * v.tone->pbMax;
        else if (b < 0) semis += b / 8192.0 * v.tone->pbMin;
        const double p = 4096.0 * std::pow(2.0, semis / 12.0);
        v.pitch = (int)std::min(std::max(std::lround(p), 1L), 0x3FFFL);
    }

    // --- reverb (psx-spx "Reverb Formula"), Room preset from the libspu
    // table at SLUS_005.51 0x800adde8 + 1 * 0x44. Addresses are in 8-byte
    // units; the buffer is in 16-bit words, so every offset is * 4.
    static int sat(int v) { return clamp16(v); }
    static int mul(int a, int b) { return (a * b) >> 15; }
    int& rv(int off) {
        int i = (m_revCur + off) % kRevWords;
        if (i < 0) i += kRevWords;
        return m_rev[i];
    }

    void reverbTick(int inL, int inR, int& outL, int& outR) {
        const int dAPF1 = 0x007D * 4, dAPF2 = 0x005B * 4;
        const int vIIR = 0x6D80, vCOMB1 = 0x54B8, vCOMB2 = (int16_t)0xBED0;
        const int vWALL = (int16_t)0xBA80, vAPF1 = 0x5800, vAPF2 = 0x5300;
        const int mLSAME = 0x04D6 * 4, mRSAME = 0x0333 * 4;
        const int mLCOMB1 = 0x03F0 * 4, mRCOMB1 = 0x0227 * 4;
        const int mLCOMB2 = 0x0374 * 4, mRCOMB2 = 0x01EF * 4;
        const int dLSAME = 0x0334 * 4, dRSAME = 0x01B5 * 4;
        const int mLAPF1 = 0x01B4 * 4, mRAPF1 = 0x0136 * 4;
        const int mLAPF2 = 0x00B8 * 4, mRAPF2 = 0x005C * 4;
        const int vLIN = (int16_t)0x8000, vRIN = (int16_t)0x8000;
        // SsUtSetReverbDepth(0x1e, 0x1e) in the audio init (0x80050af8):
        // depth * 0x7fff / 0x7f.
        const int EVOL = 0x1E * 0x7FFF / 0x7F;

        const int Lin = sat(mul(vLIN, inL));
        const int Rin = sat(mul(vRIN, inR));
        // Same side reflection.
        {
            const int prev = rv(mLSAME - 1);
            rv(mLSAME) = sat(mul(sat(Lin + mul(rv(dLSAME), vWALL) - prev), vIIR) + prev);
        }
        {
            const int prev = rv(mRSAME - 1);
            rv(mRSAME) = sat(mul(sat(Rin + mul(rv(dRSAME), vWALL) - prev), vIIR) + prev);
        }
        // Different side reflection. Room's mLDIFF/mRDIFF/dLDIFF/dRDIFF are
        // all zero, so both land on the current address (and are overwritten
        // by the SAME stage eight ticks later, before anything reads them).
        {
            const int prevL = rv(-1);
            rv(0) = sat(mul(sat(Lin + mul(rv(0), vWALL) - prevL), vIIR) + prevL);
            const int prevR = rv(-1);
            rv(0) = sat(mul(sat(Rin + mul(rv(0), vWALL) - prevR), vIIR) + prevR);
        }
        int Lout = sat(mul(vCOMB1, rv(mLCOMB1)) + mul(vCOMB2, rv(mLCOMB2)));
        int Rout = sat(mul(vCOMB1, rv(mRCOMB1)) + mul(vCOMB2, rv(mRCOMB2)));
        Lout = sat(Lout - mul(vAPF1, rv(mLAPF1 - dAPF1)));
        rv(mLAPF1) = Lout;
        Lout = sat(mul(Lout, vAPF1) + rv(mLAPF1 - dAPF1));
        Rout = sat(Rout - mul(vAPF1, rv(mRAPF1 - dAPF1)));
        rv(mRAPF1) = Rout;
        Rout = sat(mul(Rout, vAPF1) + rv(mRAPF1 - dAPF1));
        Lout = sat(Lout - mul(vAPF2, rv(mLAPF2 - dAPF2)));
        rv(mLAPF2) = Lout;
        Lout = sat(mul(Lout, vAPF2) + rv(mLAPF2 - dAPF2));
        Rout = sat(Rout - mul(vAPF2, rv(mRAPF2 - dAPF2)));
        rv(mRAPF2) = Rout;
        Rout = sat(mul(Rout, vAPF2) + rv(mRAPF2 - dAPF2));
        outL = sat(mul(Lout, EVOL));
        outR = sat(mul(Rout, EVOL));
        m_revCur = (m_revCur + 1) % kRevWords;
    }

    static int fir(const int* hist, int newest) {
        // hist is a 64-entry ring; `newest` is the index of the latest sample.
        int acc = 0;
        for (int k = 0; k < 39; ++k) acc += kReverbFir[k] * hist[(newest - k) & 63];
        return acc >> 15;
    }

    // 44.1 kHz in/out around the 22.05 kHz reverb unit, through the FIR.
    void reverb(int inL, int inR, int& outL, int& outR) {
        m_inPos = (m_inPos + 1) & 63;
        m_inL[m_inPos] = inL;
        m_inR[m_inPos] = inR;
        int yl = 0, yr = 0;
        if (m_phase) {
            reverbTick(clamp16(fir(m_inL, m_inPos)), clamp16(fir(m_inR, m_inPos)),
                       yl, yr);
        }
        m_phase ^= 1;
        m_upPos = (m_upPos + 1) & 63;
        m_upL[m_upPos] = yl;
        m_upR[m_upPos] = yr;
        // Zero-stuffed upsampling halves the level; the FIR's DC gain is 1.
        outL = clamp16(2 * fir(m_upL, m_upPos));
        outR = clamp16(2 * fir(m_upR, m_upPos));
    }

    const Vab& m_vab;
    int m_seqVol;
    Voice m_voices[kVoices];
    Channel m_chan[16];
    uint64_t m_age = 0;
    int m_notes = 0;
    std::vector<int> m_rev;
    int m_revCur = 0;
    int m_inL[64] = {}, m_inR[64] = {}, m_upL[64] = {}, m_upR[64] = {};
    int m_inPos = 0, m_upPos = 0;
    int m_phase = 0;
};

// One SEP track's event stream.
struct TrackData {
    int timeBase = 48;
    uint32_t tempo = 500000;  // microseconds per beat
    const uint8_t* ev = nullptr;
    size_t size = 0;
};

bool sepTracks(const uint8_t* d, size_t n, std::vector<TrackData>* out) {
    if (n < 8 || std::memcmp(d, "pQES", 4) != 0) return false;
    size_t pos = 8;
    bool first = true;
    while (pos + 11 <= n) {
        if (!first) pos += 2;
        if (pos + 11 > n) break;
        TrackData t;
        t.timeBase = (d[pos] << 8) | d[pos + 1];
        t.tempo = ((uint32_t)d[pos + 2] << 16) | ((uint32_t)d[pos + 3] << 8) | d[pos + 4];
        const uint32_t size = ((uint32_t)d[pos + 7] << 24) | ((uint32_t)d[pos + 8] << 16) |
                              ((uint32_t)d[pos + 9] << 8) | d[pos + 10];
        if (pos + 11 + size > n) break;
        t.ev = d + pos + 11;
        t.size = size;
        out->push_back(t);
        pos += 11 + size;
        first = false;
        if (size == 0) break;
    }
    return !out->empty();
}

}  // namespace

// SEP playback. AudioSepOpen (SLUS_005.51 0x8005156c) plays each of the
// three tracks at volume 0x5F (DAT_800b7f98..). The events are MIDI-like
// with running status; note-off is 9n with velocity 0. libsd's loop markers
// are NRPN: CC99 = 20 opens the loop (the count follows as CC6 or CC98, 127 =
// forever) and CC99 = 30 jumps back to it.
bool renderSepTrack(const uint8_t* hsb, size_t size, int track,
                    RenderedTrack* out, std::string* error) {
    if (size < 0x830) {
        if (error) *error = "HSB too small";
        return false;
    }
    const uint32_t bodyOff = rd32(hsb + size - 12);
    const uint32_t seqOff = rd32(hsb + size - 8);
    if (seqOff >= size || bodyOff > size || seqOff >= bodyOff) {
        if (error) *error = "bad HSB trailer";
        return false;
    }
    Vab vab;
    if (!vab.parse(hsb, seqOff, hsb + bodyOff, size - bodyOff, error)) return false;
    std::vector<TrackData> tracks;
    if (!sepTracks(hsb + seqOff, bodyOff - seqOff, &tracks) ||
        track >= (int)tracks.size()) {
        if (error) *error = "no such SEP track";
        return false;
    }
    const TrackData& td = tracks[track];
    Synth syn(vab, 0x5F);

    const double kRate = 44100.0;
    uint32_t tempo = td.tempo ? td.tempo : 500000;
    const int tb = td.timeBase ? td.timeBase : 48;
    auto samplesPerTick = [&]() { return kRate * tempo / 1e6 / tb; };

    size_t p = 0;
    int running = 0;
    double nextAt = 0.0;     // sample time of the next event
    bool haveNext = false;
    size_t loopPtr = 0;
    int loopRunning = 0;
    long loopStartAt = -1;   // first time the loop-start marker was reached
    long jump1 = -1, jump2 = -1;
    long endAt = -1;
    const long kCap = (long)(kRate * 900);  // 15 minutes, a runaway guard

    auto readDelta = [&](double from) -> bool {
        if (p >= td.size) return false;
        uint32_t delta = 0;
        while (p < td.size) {
            const uint8_t b = td.ev[p++];
            delta = (delta << 7) | (b & 0x7F);
            if (!(b & 0x80)) break;
        }
        nextAt = from + delta * samplesPerTick();
        return true;
    };

    haveNext = readDelta(0.0);
    std::vector<int32_t> pcm;
    pcm.reserve((size_t)kRate * 2 * 120);
    long pos = 0;
    long tailFrom = -1;
    long quiet = 0;
    while (pos < kCap) {
        // Events due at or before this frame.
        while (haveNext && endAt < 0 && nextAt <= (double)pos) {
            const double evTime = nextAt;
            if (p >= td.size) { haveNext = false; break; }
            int s = td.ev[p];
            if (s < 0x80) {
                s = running;
            } else {
                ++p;
            }
            running = s;
            const int hi = s & 0xF0, ch = s & 0x0F;
            if (hi == 0x90 || hi == 0x80 || hi == 0xA0 || hi == 0xB0 || hi == 0xE0) {
                if (p + 2 > td.size) { haveNext = false; break; }
                const int d1 = td.ev[p], d2 = td.ev[p + 1];
                p += 2;
                if (hi == 0x90 && d2 > 0) {
                    syn.noteOn(ch, d1, d2);
                } else if (hi == 0x90 || hi == 0x80) {
                    syn.noteOff(ch, d1);
                } else if (hi == 0xB0) {
                    if (d1 == 99 && d2 == 20) {
                        if (loopStartAt < 0) loopStartAt = pos;
                        loopPtr = p;
                        loopRunning = running;
                    } else if (d1 == 99 && d2 == 30 && loopStartAt >= 0) {
                        if (jump1 < 0) {
                            jump1 = pos;
                        } else {
                            jump2 = pos;
                            break;
                        }
                        p = loopPtr;
                        running = loopRunning;
                    } else {
                        syn.control(ch, d1, d2);
                    }
                } else if (hi == 0xE0) {
                    syn.bend(ch, (d2 << 7) | d1);
                }
            } else if (hi == 0xC0 || hi == 0xD0) {
                if (p + 1 > td.size) { haveNext = false; break; }
                if (hi == 0xC0) syn.program(ch, td.ev[p]);
                p += 1;
            } else if (s == 0xFF) {
                if (p >= td.size) { haveNext = false; break; }
                const int type = td.ev[p++];
                if (type == 0x51 && p + 3 <= td.size) {
                    tempo = ((uint32_t)td.ev[p] << 16) | ((uint32_t)td.ev[p + 1] << 8) |
                            td.ev[p + 2];
                    if (!tempo) tempo = 500000;
                    p += 3;
                } else {
                    endAt = pos;   // 2F end of track, or anything unknown
                    break;
                }
            } else {
                endAt = pos;
                break;
            }
            haveNext = readDelta(evTime);
            if (!haveNext) endAt = pos;
        }
        if (jump2 >= 0) break;
        if (endAt >= 0 && tailFrom < 0) {
            // SsSepStop keys every voice off; let the releases and the reverb
            // ring out.
            syn.allOff();
            tailFrom = pos;
        }
        int l, r;
        syn.render(l, r);
        pcm.push_back(l);
        pcm.push_back(r);
        ++pos;
        if (tailFrom >= 0) {
            quiet = (std::abs(l) < 2 && std::abs(r) < 2 && syn.silent()) ? quiet + 1 : 0;
            if (quiet > 4410 || pos - tailFrom > (long)(kRate * 8)) break;
        }
    }

    out->rate = (int)kRate;
    out->notes = syn.notes();
    out->mix.clear();
    if (jump2 >= 0 && jump1 >= 0 && loopStartAt >= 0) {
        // intro + the loop's second pass (its start carries the first pass's
        // tails, which is exactly what it will be preceded by on every repeat).
        out->mix.assign(pcm.begin(), pcm.begin() + loopStartAt * 2);
        out->mix.insert(out->mix.end(), pcm.begin() + jump1 * 2,
                           pcm.begin() + jump2 * 2);
        out->loopStart = loopStartAt;
        out->loopEnd = loopStartAt + (jump2 - jump1);
    } else {
        out->mix = std::move(pcm);
        out->loopStart = out->loopEnd = -1;
    }
    return true;
}

}  // namespace spu
}  // namespace re1
