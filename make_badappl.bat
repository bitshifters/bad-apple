@echo off
beebasm.exe -i 6502\m7vplay.6502 -v
call make_mode7_disks.bat badappl
