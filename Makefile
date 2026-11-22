# Wrapper so that `make` works from PowerShell / Windows Terminal.
# GNU make runs recipes with cmd.exe here (no sh on PATH).
.PHONY: all build help
all: build

build:
	cmd /c .\make.bat

help:
	@echo make        - build badappl\disks\badappl_disk1.ssd, _disk2.ssd and badappl.dsd (runs make.bat:
	@echo               beebasm 6502\m7vplay.6502, then make_mode7_disks.bat adds the frame data with bbcim)
	@echo make help   - this list
	@echo run.bat launches the demo in an emulator; the mode7video encoder is the VS solution mode7video.sln
