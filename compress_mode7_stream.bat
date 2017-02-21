REM Usage: compress_mode7_stream <short name/dir>
@echo off
DEL /Q %1\files\*
bin\exomizer.exe raw -m 3072 -c %1\%1_beeb.bin -o %1\%1_beeb.bin.exo
rem 186880 is the first 73 tracks worth of video (2560 x 73)
rem first 7 tracks of the disk are reserved
bin\split.exe --byte=186880 --numeric-suffixes %1\%1_beeb.bin.exo %1\files\%1_beeb_

rem or first 6 tracks variant
rem bin\split.exe --byte=189440 --numeric-suffixes %1\%1_beeb.bin.exo %1\files\%1_beeb_


FOR %%F IN (%1\files\*) DO echo $.%1	000000 000000 > %%F.inf
