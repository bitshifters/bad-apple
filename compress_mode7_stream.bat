REM Usage: compress_mode7_stream <short name/dir>
@echo off
bin\exomizer.exe raw -m 4096 -c %1\%1_beeb.bin -o %1\%1_beeb.bin.exo
bin\split.exe --bytes=199680 --numeric-suffixes %1\%1_beeb.bin.exo %1\files\%1_beeb_
FOR %%F IN (%1\files\*) DO echo $.%1	000000 000000 > %%F.inf
