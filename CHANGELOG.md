# Changelog

## [1.3.0](https://github.com/ecruells/resident-evil-pc-decomp/compare/residentevil-v1.2.0...residentevil-v1.3.0) (2026-09-27)


### Features

* added ogg audio support and ps1 audio migration ([47c653c](https://github.com/ecruells/resident-evil-pc-decomp/commit/47c653cae14e52088f97d0c471e7fa8857085949))
* added ps1 far object culling ([758cb3b](https://github.com/ecruells/resident-evil-pc-decomp/commit/758cb3b7a1e96005024b83fbdf060062c22aba98))
* added skip unskippable fmvs game flag ([8e78091](https://github.com/ecruells/resident-evil-pc-decomp/commit/8e780918dd3f31e096f083a64d23d597808fef92))
* asset migration tool added ([17ecd2c](https://github.com/ecruells/resident-evil-pc-decomp/commit/17ecd2cb73625d6402d31589eda548896294a34a))
* implemented directors cut mode support ([b189829](https://github.com/ecruells/resident-evil-pc-decomp/commit/b189829015eb5433d447382224a84974ca7c2549))
* implemented mp4 video playback ([298dd14](https://github.com/ecruells/resident-evil-pc-decomp/commit/298dd14f5c03300ef0345284dc9361224fe11954))
* port PS1 credit roll overlay system ([0abfc5a](https://github.com/ecruells/resident-evil-pc-decomp/commit/0abfc5a78e96cf72a37d7c0e32c3fdff8cc7cc90))
* port Ps1 Japanese prologue fmv subtitle subsystem ([f52286b](https://github.com/ecruells/resident-evil-pc-decomp/commit/f52286bae4b5b6aa8fa06525f3454e77db302a56))
* update dc migration to only import exclusive assets not present in PC ([a7c5835](https://github.com/ecruells/resident-evil-pc-decomp/commit/a7c5835784e78176c23c70f975ecfe3eaea079bb))


### Bug Fixes

* 2f mansion map bug regression ([72cf8f6](https://github.com/ecruells/resident-evil-pc-decomp/commit/72cf8f6eb611afd951c1cac3508e283b154a2288))
* asset migrator build error ([4d5a29d](https://github.com/ecruells/resident-evil-pc-decomp/commit/4d5a29d88a1f35c7037b55290211ac72f50b0b51))
* asset migrator build error ([8ec9df0](https://github.com/ecruells/resident-evil-pc-decomp/commit/8ec9df00404706be079cf1505851ae5b01342f6c))
* items test using ascii codes instead of items id constants ([9f286d6](https://github.com/ecruells/resident-evil-pc-decomp/commit/9f286d6a3efa903e75ec7cf082d6d28ca352f69c))
* JPN version using wrong title sound bank ([cdb5046](https://github.com/ecruells/resident-evil-pc-decomp/commit/cdb5046e4fb5f3db1d5e269f2c57ccebbbb61c42))
* masking issues in outfit change transition for arrange outfit change ([9131ed2](https://github.com/ecruells/resident-evil-pc-decomp/commit/9131ed2a8dbbe3ee73dcb6e8370ed0004c5451ee))
* no beretta custom in saved new game after ending an advanced run ([5a8809f](https://github.com/ecruells/resident-evil-pc-decomp/commit/5a8809f745322bd3d24ece684263b3663ae8da15))
* offset and bad glyphs in ending result screen values for JPN version ([c251ddd](https://github.com/ecruells/resident-evil-pc-decomp/commit/c251dddbcee96889d31b62242a75e1c9c6612dcd))
* str to mp4 timing issues ([ace7967](https://github.com/ecruells/resident-evil-pc-decomp/commit/ace79670757a86fbe7d1e0e7061d0ec1e595ca8f))
* videos playback and DC mode path error on linux ([fb9cfcc](https://github.com/ecruells/resident-evil-pc-decomp/commit/fb9cfcc8505f6cd336bd4ababb59665e03d4ce52))
* wrong ingram and minimi names and descriptions in DC mode ([7fa097c](https://github.com/ecruells/resident-evil-pc-decomp/commit/7fa097cb7c1b45ec9cedf216f95a2df2633d6949))

## [1.2.0](https://github.com/ecruells/resident-evil-pc-decomp/compare/residentevil-v1.1.1...residentevil-v1.2.0) (2026-09-10)


### Features

* added JPN version F9 menu texts ([fc0b0dc](https://github.com/ecruells/resident-evil-pc-decomp/commit/fc0b0dc1724467107c700bdf776cfd43a809069d))


### Bug Fixes

* implement JPN file reader exclusive background images ([867f6c9](https://github.com/ecruells/resident-evil-pc-decomp/commit/867f6c9e68bb7c43b21b5cb2a64403d92f4b0d36))

## [1.1.1](https://github.com/ecruells/resident-evil-pc-decomp/compare/residentevil-v1.1.0...residentevil-v1.1.1) (2026-09-10)


### Bug Fixes

* rename linux package to tar.gz ([e8f44e1](https://github.com/ecruells/resident-evil-pc-decomp/commit/e8f44e157b0e620b9f198ce2fa86d688bc98e6d0))

## [1.1.0](https://github.com/ecruells/resident-evil-pc-decomp/compare/residentevil-v1.0.1...residentevil-v1.1.0) (2026-09-10)


### Features

* linux ci fix ([394f588](https://github.com/ecruells/resident-evil-pc-decomp/commit/394f588f92c4139fe6ecdbac57ff336558fcc1d5))
* linux port ([178bffe](https://github.com/ecruells/resident-evil-pc-decomp/commit/178bffe5db0bcd6b5159cd464743466608ba7f52))
* update release ci to add linux build ([1232d0a](https://github.com/ecruells/resident-evil-pc-decomp/commit/1232d0a5141946a769b630bf49bb930dbd916776))

## [1.0.1](https://github.com/ecruells/resident-evil-pc-decomp/compare/residentevil-v1.0.0...residentevil-v1.0.1) (2026-09-08)


### Bug Fixes

* bug that breaks character state doing quick load between different characters ([e962857](https://github.com/ecruells/resident-evil-pc-decomp/commit/e962857ceb6597537034281062cdc83668d875ce))
* bug that makes chimeras warp when you shot them and fall from ceiling ([cb11a87](https://github.com/ecruells/resident-evil-pc-decomp/commit/cb11a87c756757aa71834ecddce066a201b7f698))
* debug menu off-screen and bad contrast texts ([dbc62ba](https://github.com/ecruells/resident-evil-pc-decomp/commit/dbc62bae69d70859925a2107e8ec9eda68985ead))
* misplacement when changing rooms in certains rooms ([fe98b42](https://github.com/ecruells/resident-evil-pc-decomp/commit/fe98b42c2cef76d0a63ce0ab2964562215383af3))
* missing jills hand animation in computer lab subsystem ([6d0cbb9](https://github.com/ecruells/resident-evil-pc-decomp/commit/6d0cbb9aa7e3ccae7c824be067a3a9188e5bf234))
* shadows drawing above bg masks ([80c924e](https://github.com/ecruells/resident-evil-pc-decomp/commit/80c924e16003db0f8736981a054a6ca9d87a4cf3))

## 1.0.0 (2026-09-08)


### Features

* add release workflow with release-please versioning ([2ffc181](https://github.com/ecruells/resident-evil-pc-decomp/commit/2ffc1813051c0616288a174baa3fe6737c6ff028))
* added jap saveload menu strings and layout ([7d46c59](https://github.com/ecruells/resident-evil-pc-decomp/commit/7d46c596720c927d8a7da14ad76561e0e4b1db41))
* added support for japanese text encoding and globals texts ([9ffe0bd](https://github.com/ecruells/resident-evil-pc-decomp/commit/9ffe0bd28abfb9493d1550935b7f2a0cf1dc8d49))
* implement debug menu in the spirit of classic rebirth one ([2c83e4f](https://github.com/ecruells/resident-evil-pc-decomp/commit/2c83e4f4f8de0f733f27478759e099d85daff806))
* implement xinput support ([e994317](https://github.com/ecruells/resident-evil-pc-decomp/commit/e994317d6bd9a122684936469f6ee26f87b7f062))


### Bug Fixes

* 2f map canvas not fading out when use lighter ([dace6db](https://github.com/ecruells/resident-evil-pc-decomp/commit/dace6db6d1431b89032a8c1ba7686bbfc93b13fb))
* add missing emd ([4bc18bb](https://github.com/ecruells/resident-evil-pc-decomp/commit/4bc18bbe12c7d7dab76453ee90d44d6daa988f14))
* crash opening the options menu in release build ([14b9e08](https://github.com/ecruells/resident-evil-pc-decomp/commit/14b9e08fb277ca4ba7f36532fb643be526484a2c))
* events not waiting voice playing end ([e2a87ef](https://github.com/ecruells/resident-evil-pc-decomp/commit/e2a87ef170935a1f3f018b283b8129d5a3ca8ea4))
* incorrect itembox list frame elements positions ([d40dece](https://github.com/ecruells/resident-evil-pc-decomp/commit/d40dece73c9a815fb3b077f720379272595a9afd))
* incorrect minimi aim angles ([e259700](https://github.com/ecruells/resident-evil-pc-decomp/commit/e2597009f48635a4e60e1164cf01f54ffa5f3470))
* missing swallowed animation ([eb73481](https://github.com/ecruells/resident-evil-pc-decomp/commit/eb734814303f29455896139359b5d8ff4f277128))
* no blood effect in hunter slashing rebecca ([01cd0b3](https://github.com/ecruells/resident-evil-pc-decomp/commit/01cd0b3f7dfa91d5e4da009470f24e4e7b0599e8))
* no gory explosion effect rendering ([4e6d066](https://github.com/ecruells/resident-evil-pc-decomp/commit/4e6d0667a396ddea6a8a29b91a88ba7fd8baf214))
* no voices audio in underground barrys death scene ([f134762](https://github.com/ecruells/resident-evil-pc-decomp/commit/f134762f58d00d178f809ff657db6b276133ff91))
* odd web spinner behaviour ([5e25675](https://github.com/ecruells/resident-evil-pc-decomp/commit/5e256753e833d777a9984f38e7825c7cff6375be))
* passcode panels light always red colored when passcode inserted instead of blue ([5725961](https://github.com/ecruells/resident-evil-pc-decomp/commit/5725961b8abce280a1ace1d7607b20cce8b1f018))
* regenerated all 896 bytes of the table for effect sprites ([cde96c4](https://github.com/ecruells/resident-evil-pc-decomp/commit/cde96c436a787f80c6a9e61dd92f18344fe8a7e4))
* remove sparkle effect when picking item ([a419b98](https://github.com/ecruells/resident-evil-pc-decomp/commit/a419b980c0fe60385cca86abb9ce4de8c763145d))
* rename misnormer depth to texturePage ([020e2b5](https://github.com/ecruells/resident-evil-pc-decomp/commit/020e2b589b671053d62cf4b716576f6cb116eeb8))
* texture bankid and cell bug ([a5b25c8](https://github.com/ecruells/resident-evil-pc-decomp/commit/a5b25c8271dfc244ab9a7208aff9951e8249161e))
* textureDesc misnormer properties ([56dceda](https://github.com/ecruells/resident-evil-pc-decomp/commit/56dceda56b721c2b2b00cedb8b83d7221d34f47b))
* wasp red blood effect bug ([db328d9](https://github.com/ecruells/resident-evil-pc-decomp/commit/db328d9683c8fb2379315e5e6abec5dc6debc679))
* water tank transparent in study 2f ([288468b](https://github.com/ecruells/resident-evil-pc-decomp/commit/288468b6db59bec12e46282c0bc3af2a3cdc0783))
* wrong sweep color when healing ([922e344](https://github.com/ecruells/resident-evil-pc-decomp/commit/922e344eacff9dcf1e5a2a2e8e057146cc1ead1f))
* wrong weapons reload anim and implement missing falling magazine anim ([5e91705](https://github.com/ecruells/resident-evil-pc-decomp/commit/5e917050270ec95763af67837a344cb40a55f819))
* zombies vomit not rendering ([3644404](https://github.com/ecruells/resident-evil-pc-decomp/commit/364440465b83eaddd114e80e4d16c362f6ba994a))
