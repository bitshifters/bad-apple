@echo off
beebasm.exe -v -i 6502\m7vplay.6502 -do badappl/disks/badappl_disk1.ssd -opt 2 -title "BAD APPLE"



call make_mode7_disks.bat badappl
