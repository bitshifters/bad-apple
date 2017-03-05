@echo off
..\..\Bin\beebasm.exe -v -i 6502\m7vplay.6502 -do badappl/disks/badappl_disk1.ssd -opt 2 -title "BAD APPLE" > compile.txt
make_mode7_disks badappl
