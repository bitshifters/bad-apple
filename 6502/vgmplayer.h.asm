
\\ VGM Player module
\\ Include file
\\ Define ZP and constant vars only in here
\\ Uses memory &0380-&03DF & ZP


\ ******************************************************************
\ *	Define global constants
\ ******************************************************************

\\ Player
VGM_PLAYER_string_max = 42			; size of our meta data strings (title and author)
VGM_PLAYER_sample_rate = 50			; locked to 50Hz

\ ******************************************************************
\ *	Declare ZP variables
\ ******************************************************************

\\ Frequency array for vu-meter effect, plus beat bars for 4 channels
\\ These two must be contiguous in memory

\\ Player vars
.vgm_player_ended			SKIP 1		; non-zero when player has reached end of tune
.vgm_player_lock			SKIP 1		; stop music player being re-entered
