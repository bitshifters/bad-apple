
\ ******************************************************************
\ *	Exomiser (decompression library)
\ ******************************************************************

.MUS_start

; -------------------------------------------------------------------
; Unpack a compressed data stream previously initialized by MUS_init_decruncher
; to the memory address specified in X,Y
.MUS_unpack
{
	STX write_chr+1
	STY write_chr+2

	.next_chr
	JSR MUS_get_decrunched_byte
	BCS all_done
	.write_chr	STA &ffff				; **SELF-MODIFIED**
	INC write_chr+1
	BNE next_chr
	INC write_chr+2
	BNE next_chr
	.all_done
	RTS
}

; -------------------------------------------------------------------
; Fetch byte from an exomiser compressed data stream
; for this MUS_get_crunched_byte routine to work the crunched data has to be
; crunced using the -m <buffersize> and possibly the -l flags. Any other
; flag will just mess things up.
.MUS_get_crunched_byte
{

._byte
	lda &ffff ; EXO data stream address	; **SELF-MODIFIED CODE**
_byte_lo = _byte + 1
_byte_hi = _byte + 2

	\\ advance input stream memory address
	INC _byte_lo
	bne _byte_skip_hi
	INC _byte_hi			; forward decrunch
._byte_skip_hi:

	rts						; decrunch_file is called.
}

; -------------------------------------------------------------------


MUS_crunch_byte_lo = MUS_get_crunched_byte + 1
MUS_crunch_byte_hi = MUS_get_crunched_byte + 2



; -------------------------------------------------------------------
; jsr this label to init the decruncher, it will init used zeropage
; zero page locations and the decrunch tables
; no constraints on register content, however the
; decimal flag has to be #0 (it almost always is, otherwise do a cld)
; X/Y contains address of EXO crunched data stream
; -------------------------------------------------------------------
.MUS_init_decruncher				; pass in address of (crunched data-1) in X,Y
{
	stx MUS_crunch_byte_lo
	sty MUS_crunch_byte_hi

	jsr MUS_get_crunched_byte
	sta MUS_zp_bitbuf

	ldx #0
	stx MUS_zp_dest_lo
	stx MUS_zp_dest_hi
	stx MUS_zp_len_lo
	stx MUS_zp_len_hi
	ldy #0
; -------------------------------------------------------------------
; calculate tables (49 bytes)
; x and y must be #0 when entering
;
._init_nextone
	inx
	tya
	and #$0f
	beq _init_shortcut		; starta p� ny sekvens

	txa			; this clears reg a
	lsr a			; and sets the carry flag
	ldx MUS_zp_bits_lo
._init_rolle
	rol a
	rol MUS_zp_bits_hi
	dex
	bpl _init_rolle		; c = 0 after this (rol MUS_zp_bits_hi)

	adc MUS_tabl_lo-1,y
	tax

	lda MUS_zp_bits_hi
	adc MUS_tabl_hi-1,y
._init_shortcut
	sta MUS_tabl_hi,y
	txa
	sta MUS_tabl_lo,y

	ldx #4
	jsr MUS_bit_get_bits		; clears x-reg.
	sta MUS_tabl_bi,y
	iny
	cpy #52
	bne _init_nextone
}
\\ Fall through!	

.MUS_do_exit
	rts

; -------------------------------------------------------------------
; decrunch one byte
;
.MUS_get_decrunched_byte
{
	ldy MUS_zp_len_lo
	bne _do_sequence
	ldx MUS_zp_len_hi
	bne _do_sequence2

	jsr MUS_bit_get_bit1
	beq _get_sequence
; -------------------------------------------------------------------
; literal handling (13 bytes)
;
	jsr MUS_get_crunched_byte
	bcc _do_literal
; -------------------------------------------------------------------
; count zero bits + 1 to get length table index (10 bytes)
; y = x = 0 when entering
;
._get_sequence
._seq_next1
	iny
	jsr MUS_bit_get_bit1
	beq _seq_next1
	cpy #$11
	bcs MUS_do_exit
; -------------------------------------------------------------------
; calulate length of sequence (zp_len) (17 bytes)
;
	ldx MUS_tabl_bi - 1,y
	jsr MUS_bit_get_bits
	adc MUS_tabl_lo - 1,y
	sta MUS_zp_len_lo
	lda MUS_zp_bits_hi
	adc MUS_tabl_hi - 1,y
	sta MUS_zp_len_hi
; -------------------------------------------------------------------
; here we decide what offset table to use (20 bytes)
; x is 0 here
;
	bne _seq_nots123
	ldy MUS_zp_len_lo
	cpy #$04
	bcc _seq_size123
._seq_nots123
	ldy #$03
._seq_size123
	ldx MUS_tabl_bit - 1,y
	jsr MUS_bit_get_bits
	adc MUS_tabl_off - 1,y
	tay
; -------------------------------------------------------------------
; calulate absolute offset (zp_src) (27 bytes)
;
	ldx MUS_tabl_bi,y
	jsr MUS_bit_get_bits;
	adc MUS_tabl_lo,y
	bcc _seq_skipcarry
	inc MUS_zp_bits_hi
	clc
._seq_skipcarry
	adc MUS_zp_dest_lo
	sta MUS_zp_src_lo
	lda MUS_zp_bits_hi
	adc MUS_tabl_hi,y
	adc MUS_zp_dest_hi
; -------------------------------------------------------------------
	cmp #HI(MUS_buffer_len)
	bcc _seq_offset_ok
	sbc #HI(MUS_buffer_len)
	clc
; -------------------------------------------------------------------
._seq_offset_ok
	sta MUS_zp_src_hi
	adc #HI(MUS_buffer_start)
	sta MUS_zp_src_bi
._do_sequence
	ldy #0
._do_sequence2
	ldx MUS_zp_len_lo
	bne _seq_len_dec_lo
	dec MUS_zp_len_hi
._seq_len_dec_lo
	dec MUS_zp_len_lo
; -------------------------------------------------------------------
	ldx MUS_zp_src_lo
	bne _seq_src_dec_lo
	ldx MUS_zp_src_hi
	bne _seq_src_dec_hi
; ------- handle buffer wrap problematics here ----------------------
	ldx #HI(MUS_buffer_len)
	stx MUS_zp_src_hi
	ldx #HI(MUS_buffer_end)
	stx MUS_zp_src_bi
; -------------------------------------------------------------------
._seq_src_dec_hi
	dec MUS_zp_src_hi
	dec MUS_zp_src_bi
._seq_src_dec_lo
	dec MUS_zp_src_lo
; -------------------------------------------------------------------
	lda (MUS_zp_src_lo),y
; -------------------------------------------------------------------
._do_literal
	ldx MUS_zp_dest_lo
	bne _seq_dest_dec_lo
	ldx MUS_zp_dest_hi
	bne _seq_dest_dec_hi
; ------- handle buffer wrap problematics here ----------------------
	ldx #HI(MUS_buffer_len)
	stx MUS_zp_dest_hi
	ldx #HI(MUS_buffer_end)
	stx MUS_zp_dest_bi
; -------------------------------------------------------------------
._seq_dest_dec_hi
	dec MUS_zp_dest_hi
	dec MUS_zp_dest_bi
._seq_dest_dec_lo
	dec MUS_zp_dest_lo
; -------------------------------------------------------------------
	sta (MUS_zp_dest_lo),y
	clc
	rts
}

; -------------------------------------------------------------------
; two small static tables (6 bytes)
;
.MUS_tabl_bit
{
	EQUB 2,4,4
}
.MUS_tabl_off
{
	EQUB 48,32,16
}

; -------------------------------------------------------------------
; get x + 1 bits (1 byte)
;
.MUS_bit_get_bit1
	inx
; -------------------------------------------------------------------
; get bits (31 bytes)
;
; args:
;   x = number of bits to get
; returns:
;   a = #bits_lo
;   x = #0
;   c = 0
;   MUS_zp_bits_lo = #bits_lo
;   MUS_zp_bits_hi = #bits_hi
; notes:
;   y is untouched
;   other status bits are set to (a == #0)
; -------------------------------------------------------------------
.MUS_bit_get_bits
{
	lda #$00
	sta MUS_zp_bits_lo
	sta MUS_zp_bits_hi
	cpx #$01
	bcc _bit_bits_done
	lda MUS_zp_bitbuf
._bit_bits_next
	lsr a
	bne _bit_ok
	jsr MUS_get_crunched_byte
	ror a
._bit_ok
	rol MUS_zp_bits_lo
	rol MUS_zp_bits_hi
	dex
	bne _bit_bits_next
	sta MUS_zp_bitbuf
	lda MUS_zp_bits_lo
._bit_bits_done
	rts
}
; -------------------------------------------------------------------
; end of decruncher
; -------------------------------------------------------------------


.MUS_end

