\ ******************************************************************
\\ VGM Player
\\ Code module
\ ******************************************************************

VGM_PLAYER_ORG = *

ORG &0380
GUARD &03E0

\ ******************************************************************
\ *	VGM music player data area
\ ******************************************************************
.vgm_player_song_title_len	SKIP 1
; Remove this as we don't need the metadata in this demo but we do need the RAM!
;.vgm_player_song_title		SKIP VGM_PLAYER_string_max
.vgm_player_song_author_len	SKIP 1
; And this!
;.vgm_player_song_author		SKIP VGM_PLAYER_string_max

ORG VGM_PLAYER_ORG

.vgm_player_start


.tmp_var SKIP 1
.tmp_msg_idx SKIP 1

\\ Copied out of the RAW VGM header
.vgm_player_packet_count	SKIP 2		; number of packets
.vgm_player_duration_mins	SKIP 1		; song duration (mins)
.vgm_player_duration_secs	SKIP 1		; song duration (secs)

.vgm_player_packet_offset	SKIP 1		; offset from start of file to beginning of packet data



\ ******************************************************************
\ *	VGM music player routines
\ * Plays a RAW format VGM music stream from an Exomiser compressed data stream
\ ******************************************************************

\ *	EXO VGM data file

\ * This must be compressed using the following flags:
\ * exomizer.exe raw -c -m 1024 <file.raw> -o <file.exo>


\\ Initialise the VGM player with an Exomizer compressed data stream
\\ X - lo byte of data stream to be played
\\ Y - hi byte of data stream to be played
.vgm_init_stream
{
	\\ Initialise exomizer - must have some data ready to decrunch
	JSR MUS_init_decruncher

	\\ Initialise music player - parses header
	JSR	vgm_init_player

	RTS
}


.vgm_init_player				; return non-zero if error
{
\\ <header section>
\\  [byte] - header size - indicates number of bytes in header section

	LDA #1
	STA vgm_player_packet_offset

	jsr MUS_get_decrunched_byte
	STA tmp_var
	CMP #5
	BCS parse_header			; we need at least 5 bytes to parse!
	JMP error

	.parse_header
	CLC
	ADC vgm_player_packet_offset
	STA vgm_player_packet_offset

\\  [byte] - indicates the required playback rate in Hz eg. 50/60/100

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	CMP #VGM_PLAYER_sample_rate		; we only support 50Hz files
	BEQ is_50HZ					; return non-zero to indicate error
	JMP error
	.is_50HZ
	DEC tmp_var

\\  [byte] - packet count lsb

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA vgm_player_packet_count
	DEC tmp_var

\\  [byte] - packet count msb

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA vgm_player_packet_count+1
	DEC tmp_var

\\  [byte] - duration minutes

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA vgm_player_duration_mins
	DEC tmp_var

\\  [byte] - duration seconds

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA vgm_player_duration_secs

	.header_loop
	DEC tmp_var
	BEQ done_header

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	\\ don't know what this byte is so ignore it
	JMP header_loop

	.done_header

\\ <title section>
\\  [byte] - title string size

	INC vgm_player_packet_offset

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA tmp_var

	CLC
	ADC vgm_player_packet_offset
	STA vgm_player_packet_offset

\\  [dd] ... - ZT title string

	LDX #0
	.title_loop
	STX tmp_msg_idx
	LDA tmp_var
	BEQ done_title				; make sure we consume all the title string
	DEC tmp_var

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	LDX tmp_msg_idx
	CPX #VGM_PLAYER_string_max
	BCS title_loop				; don't write if buffer full
; Parse but don't store the metadata as we need the RAM
;	STA vgm_player_song_title,X
	INX
	JMP title_loop

	\\ Where title string is smaller than our buffer
	.done_title
	STX vgm_player_song_title_len
	LDA #' '
	.title_pad_loop
	CPX #VGM_PLAYER_string_max
	BCS done_title_padding
; Parse but don't store the metadata as we need the RAM
;	STA vgm_player_song_title,X
	INX
	JMP title_pad_loop
	.done_title_padding

\\ <author section>
\\  [byte] - author string size

	INC vgm_player_packet_offset

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	STA tmp_var

	CLC
	ADC vgm_player_packet_offset
	STA vgm_player_packet_offset

\\  [dd] ... - ZT author string

	LDX #0
	.author_loop
	STX tmp_msg_idx
	LDA tmp_var
	BEQ done_author				; make sure we consume all the author string
	DEC tmp_var

	jsr MUS_get_decrunched_byte		; should really check carry status for EOF
	LDX tmp_msg_idx
	CPX #VGM_PLAYER_string_max
	BCS author_loop
; Parse but don't store the metadata as we need the RAM
;	STA vgm_player_song_author,X	; don't write if buffer full
	INX
	JMP author_loop

	\\ Where author string is smaller than our buffer
	.done_author
	STX vgm_player_song_author_len
	LDA #' '
	.author_pad_loop
	CPX #VGM_PLAYER_string_max
	BCS done_author_padding
; Parse but don't store the metadata as we need the RAM
;	STA vgm_player_song_author,X
	INX
	JMP author_pad_loop
	.done_author_padding

	\\ Initialise vars
	LDA #0
	STA vgm_player_ended
	STA vgm_player_lock

	\\ Return zero 
	RTS

	\\ Return error
	.error
	LDA #&FF
	RTS
}

.deinit_player
{
	\\ Zero volume on all channels
	LDA #&9F: JSR psg_strobe
	LDA #&BF: JSR psg_strobe
	LDA #&DF: JSR psg_strobe
	LDA #&FF: JSR psg_strobe
	
	.return
	RTS
}

.poll_player
{
	\\ Assume this is called every 20ms..

	\\ Are we already playing (some bad timer thing happened)

	LDA vgm_player_lock
	BNE return				; just return right away

	\\ We already finished?

	LDA vgm_player_ended
	BNE return

	\\ Stop routine re-entering

	INC vgm_player_lock

	TXA:PHA:TYA:PHA

\\ <packets section>
\\  [byte] - indicating number of data writes within the current packet (max 11)
\\  [dd] ... - data
\\  [byte] - number of data writes within the next packet
\\  [dd] ... - data
\\  ...`
\\ <eof section>
\\  [0xff] - eof

	\\ Get next byte from the stream
	jsr MUS_get_decrunched_byte
	bcs wait_20_ms

	cmp #&ff
	beq _player_end

	\\ Byte is #data bytes to send to sound chip:
	TAY
	.sound_data_loop
	BEQ wait_20_ms
	TYA:PHA
	jsr MUS_get_decrunched_byte
	bcc not_sample_end
	PLA
	JMP _player_end

	.not_sample_end
	.^vgm_player_psg_strobe
	JSR psg_strobe
	PLA:TAY:DEY
	JMP sound_data_loop
	
	._player_end
	LDA #1
	STA vgm_player_ended

	\\ Silence sound chip
	JSR deinit_player

	.wait_20_ms
	DEC vgm_player_lock

	PLA:TAY:PLA:TAX

	.return
	RTS
}

.psg_strobe
{
	sei					; **SELF-MODIFIED CODE**
	ldy #255
	sty $fe43
	
	sta $fe41
	lda #0
	sta $fe40
	nop
	nop
	nop
	nop
	nop
	nop
	lda #$08
	sta $fe40
	cli					; **SELF-MODIFIED CODE**
	RTS
}

PSG_STROBE_SEI_INSN = psg_strobe
PSG_STROBE_CLI_INSN = psg_strobe + 25

.vgm_player_end
