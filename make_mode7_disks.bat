REM Usage: make_mode7_disks <short name/dir>
@echo off

rem disk1 now created by beebasm
rem del "%1\disks\%1_disk1.ssd"
rem bin\bbcim -a "%1\disks\%1_disk1.ssd" "m7vplay"

del "%1\disks\%1_disk2.ssd"

CD files
rem boot & track padding now compiled into demo 
rem ..\bin\bbcim -a "..\%1\disks\%1_disk1.ssd" "Boot"
rem ..\bin\bbcim -a "..\%1\disks\%1_disk1.ssd" "Dummy2"
..\bin\bbcim -a "..\%1\disks\%1_disk2.ssd" "Readme"
CD "..\%1\files"
..\..\bin\bbcim -a "..\disks\%1_disk1.ssd" "%1_beeb_00"
..\..\bin\bbcim -a "..\disks\%1_disk2.ssd" "%1_beeb_01"
..\..\bin\bbcim -a "..\disks\%1_disk2.ssd" "%1_beeb_02"
CD "..\..\files"
..\bin\bbcim -a "..\%1\disks\%1_disk2.ssd" "help"
CD "..\%1\disks"
..\..\bin\bbcim -interss sd %1_disk1.ssd %1_disk2.ssd %1.dsd
CD ..\..
