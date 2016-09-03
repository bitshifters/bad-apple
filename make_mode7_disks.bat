REM Usage: make_mode7_disks <short name/dir>
@echo off
del "%1\disks\%1_disk1.ssd"
del "%1\disks\%1_disk2.ssd"
bin\bbcim -a "%1\disks\%1_disk1.ssd" "m7vplay"
CD files
..\bin\bbcim -a "..\%1\disks\%1_disk1.ssd" "Boot"
..\bin\bbcim -a "..\%1\disks\%1_disk1.ssd" "Dummy2"
..\bin\bbcim -a "..\%1\disks\%1_disk2.ssd" "Dummy8"
CD "..\%1\files"
..\..\bin\bbcim -a "..\disks\%1_disk1.ssd" "%1_beeb_00"
..\..\bin\bbcim -a "..\disks\%1_disk2.ssd" "%1_beeb_01"
CD "..\disks"
..\..\bin\bbcim -interss sd %1_disk1.ssd %1_disk2.ssd %1.dsd
CD ..\..
