# bad-apple: start here

"Bad Apple" for the BBC Micro in Teletext (MODE 7): a video codec (Windows C++ encoder) and a 6502 streaming player, 3m21s at 25 fps from one double-sided floppy. Bitshifters demo (Kieran and Simon), released 2017 (v1.2).

| | |
|---|---|
| GitHub | https://github.com/bitshifters/bad-apple (public) |
| Local path | D:\bitshifters\bad-apple (moved from OneDrive\BEEB\Repos\bad-apple in the 2026-11 tidy) |
| Branch | master; others: demo-loop, investigation-video-codec, irqv-decode, load-by-sector (all four deleted on GitHub, "gone"), music-on-irq (2017-02-25), wip/2026-11-tidy |
| Last commit | 2017-04-29: Merge pull request #14 from bitshifters/demo-loop |

## What's in here
- `6502\`: the player (`m7vplay.6502`), VGM player, Exomiser decruncher, credit scroll.
- `mode7video\` + `mode7video.sln`: the encoder (Visual Studio C++, uses `jpeg-6b\` and `lpng1624\`); `mode7video.exe/.pdb/.ilk` are checked in at the top level.
- `badappl\`: frame data and the disc images (`badappl\disks\badappl_disk1.ssd`, `_disk2.ssd`, `badappl.dsd`).
- `files\`: !Boot, readme, help and padding files for the discs. `data\`: music (VGM) and art. `bin\`: bbcim, exomizer, ffmpeg, Python helpers.
- Pipeline scripts: `rip_mode7_frames.bat`, `compress_mode7_stream.bat`, `make_mode7_disks.bat`.

## Build and run
- `make`: runs `make.bat` (Makefile added 2026-11-22): beebasm (`..\Bin\beebasm.exe`) assembles `6502\m7vplay.6502` into `badappl\disks\badappl_disk1.ssd`, then `make_mode7_disks.bat badappl` adds the frames with `bin\bbcim` and makes `badappl.dsd`. **Not run on 2026-11-22**: it rewrites the two disc images, which already have uncommitted changes.
- `make help` lists targets. `make_badappl.bat` is a near-copy of `make.bat` (uses `call`).
- Run: `run.bat` -> `run_mode7_disk.bat badappl` launches `..\..\bin\B-em\B-em.exe` on `badappl.dsd` (D:\beeb\Bin is a junction into D:\tools\beeb\Bin; check B-em is there).

## Local changes
- Uncommitted: `6502\m7vplay.6502` (one constant `LDA #1` -> `LDA #2`), `make_badappl.bat` (beebasm path and `> compile.txt`), and the rebuilt `badappl_disk1.ssd` and `badappl.dsd`.
- Untracked: `badappl\badappl_beeb.shrink` (350 KB), `data\fixed tune\` (bbcapple VGM/bin/exo), `bad apple.txt`, `badappl_deltas_as_released.zip` (13 MB).
- Branch `wip/2026-11-tidy` (pushed) holds the modified files plus `badappl_beeb.shrink` and `data\fixed tune\` (not the zip or the .txt).
- Large files in the tree: `mode7video.VC.db` (42 MB, ignored), `touhou bad apple smile.mp4` (16 MB, tracked), `mode7video.pdb` (8 MB, tracked).

## Status and next steps
- Released; no TODO/plan notes. The uncommitted "fixed tune" and the `.shrink` file suggest a later experiment (new music build, Shrinkler-packed stream); decide whether to keep it.
- D:\beeb\mine\teletext-broadcast refers to this repo (DEMO.md).

## See also
- README.md; release page https://bitshifters.github.io/posts/prods/bs-badapple.html.
