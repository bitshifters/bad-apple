REM Usage: compress_mode7_stream <short name/dir>
@echo off
DEL /Q %1\files\*
bin\exomizer.exe raw -m 4096 -c %1\%1_beeb.bin -o %1\%1_beeb.bin.exo
bin\split.exe --byte=189440 --numeric-suffixes %1\%1_beeb.bin.exo %1\files\%1_beeb_
FOR %%F IN (%1\files\*) DO echo $.%1	000000 000000 > %%F.inf
