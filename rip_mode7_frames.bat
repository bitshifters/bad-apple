REM Usage: rip_mode7_frames <input file> <output short name/dir> <geometry WxH>
@echo off
md %2
md %2\frames
md %2\bin
md %2\delta
md %2\files
md %2\disks
bin\ffmpeg.exe -i %1 -r 22.64 -s %3 %2\frames\%2-%%d.png