# Director's Cut audio / sound differences (vs OG PS1)

Comparison of the audio content and the audio tables between the Director's Cut
(`SLUS_005.51`) and the original PS1 release (`SLUS_001.70`). Companion to
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md` and `docs/PSX_ENGINE_ANALYSIS.md`.

## Verdict

**The DC's audio content is unchanged.** Every sound-effect bank, every BGM
bank and every voice stream is byte-identical; the only differing audio-bearing
files are two FMV movies, and the binary bank tables differ only by the global
file-index shift.

## Assets

| Group | OG | DC | Result |
|---|---|---|---|
| `SOUND/*.HED` + `*.VB` (35 SFX VAB banks) | 70 files | 70 files | **identical** (size + SHA-1) |
| `SOUND/*.HSB` (57 BGM banks) | 57 files | 57 files | **identical** |
| `VOICE1-5.XAS` (CD-XA voice streams) | 5 files | 5 files | **identical** |
| `MOVIE/*.STR` | 25 files | 25 files | 23 identical; `CAPCOM.STR`, `DMC.STR` differ |

So there is **no new or remixed music/SFX/voice** — in particular the DC keeps
the original soundtrack (the remix is a DualShock re-release change, not this
build).

### The two changed movies

- `MOVIE/CAPCOM.STR` — the Capcom logo movie. Different movie: header frame
  count 4 → 7, and 2,969,600 → 2,459,648 B. A re-authored logo animation.
- `MOVIE/DMC.STR` — same size (5,632,000 B), but 83 bytes differ, in two
  0x800-byte sectors (`0x4F0` and `0x528`). The differing bytes are in the
  compressed MDEC video payload (branches inside a couple of frames), not in XA
  audio sectors.

## Audio tables in the executable

`LoadAudioSample` selects the VAB head/body file by sample id
(OG `0x80051680` / DC `0x80050fc4`). The two `u16` file-index tables:

| | OG | DC |
|---|---|---|
| head file-index table | `0x80091134` | `0x80091a30` |
| body file-index table | `0x80091154` | `0x80091a50` |

Their **contents differ only by a constant −31** — the same shift the DC's
file-system table has for the SOUND files (the DC disc drops/adds files ahead of
SOUND). Every entry maps the same sample id to the same logical bank; the last
eight entries (8/12/8/8/80/120/8/132, not file indices) are identical. So the
id→sound mapping is unchanged.

## Conclusion

For the Director's Cut, the only audio differences are the re-authored Capcom
logo FMV and an 83-byte change inside `DMC.STR`'s video. SFX, BGM, voices, and
the sound-id mapping are untouched.
