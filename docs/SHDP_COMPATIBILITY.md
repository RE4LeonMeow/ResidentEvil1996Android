# SHDP compatibility — Android build 36

Use the full official RE_SHDP_1.1.zip with your own USA GOG installation.
A full pack already imported with build 33/34/35 updates automatically. The
first mod preview needs a full reimport to get its movie payloads.

## What was checked

- The phone's supplied file tree matches all 1,733 files in the official pack.
- Movie names are matched without regard to capitalization. Imported files
  are addressed by their installed ID/slot, not by a case-sensitive filename.
- All 27 compatible movie replacements are mapped to their USA GOG names.
  The original MP4 bytes, resolution and audio are preserved. A bounded-chunk
  worker copy prepares a disk-backed local video file, then WebView plays its
  blob URL. This bypasses intercepted HTTP media requests that returned
  net::ERR_FAILED on the reported Android 14 WebView.
- Capcom, Dm1, PJ (the live-action intro) and Ed4 were checked for real playback
  and seeks in desktop Chromium, including the original differing dimensions.
- 71 of the 72 original inventory icon rows have exact reference matches.
  Their authored HD pixels are cropped from SHDP's padded tiles. The renderer
  resolves the drawn 40x30 sprite inside the changing inventory/item-box atlas,
  keeping the original proportions. The actual game inventory used HD knife,
  handgun and spray sprites in the test.
- All 31 directly matching effect pages were compared byte for byte with the
  pack's HD pixels, along with the hall masks, stairs and save background.

## Known differences in the USA GOG data

The pack was authored for a different PC version. Exact reference matching
keeps a texture original when its pixel layout/palette differs; it does not
substitute a vaguely similar image.

- Inventory icon row 46 (the GOG oil can) has no exact SHDP icon match.
- Effspr/esp221.tim and Effspr/esp224.tim have different layouts from the
  supplied effect references and retain their original pages.
- 60 of 77 default first-palette Item_m2 IVM texture sheets match directly.
  The following remaining 3D item texture sheets need a separate conversion:
  I06V, I10V, I11V, I18V, I35V, I40V, I47V, I49V, I58V, I62V, I67V,
  I68V, I69V, I70V, I71V, I75V and MINI. These are filenames ending .IVM;
  they are separate from the small inventory icons fixed in this build.

Logs include actual HD item-icon/effect-page upload counts to help identify
remaining cases. Desktop checks do not confirm every scene on Android hardware;
please report the affected scene and copy the phone's diagnostic report.

Only compatibility metadata is bundled. The source/APK contains no original
GOG files, SHDP artwork/movies, private saves or test captures.
