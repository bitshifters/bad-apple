
; vertical credits scroller
; somewhat generalised so will scroll all MODE 7 characters up by one pixel at a time
; then adds new row of pixels at the bottom from a fixed array

; separate routine fills the new line buffer with font data from an array of text

.start_fx_creditscroll

\\ Change these to adjust window that is scrolled
CREDITS_shadow_addr = &7C00 + 4*40	; offset by first 4 rows (where logo+header are)
CREDITS_end_addr = CREDITS_shadow_addr + (MODE7_char_width * MODE7_char_height) - 5*40 ; less 1 line which is where test card line is
CREDITS_first_char = 1
CREDITS_last_char = MODE7_char_width

ROW_DELAY = 15	; speed of line updates in vsyncs, set to 0 for no delay

.line_counter EQUB 0


\ ******************************************************************
\ *	Credit Scroll FX
\ ******************************************************************

\\ Scrolls entire screen up by one pixel adding new pixels from array

.fx_creditscroll_scroll_up
{
	\\ Start by updating the top line
	LDA #LO(CREDITS_shadow_addr)
	STA writeptr
	LDA #HI(CREDITS_shadow_addr)
	STA writeptr+1

	\\ But we'll also be reading from the next line
	LDA #LO(CREDITS_shadow_addr + MODE7_char_width)
	STA readptr
	LDA #HI(CREDITS_shadow_addr + MODE7_char_width)
	STA readptr+1

	\\ For each character row
	.y_loop

	\\ First char in row
	LDY #CREDITS_first_char
	.x_loop

	\\ Get top pixels from row below
	LDA (readptr), Y
	AND #&3
	STA top_bits + 1

	\\ Get bottom pixels from current row
	LDA (writeptr), Y
	AND #&FC

	\\ Merge them together
	.top_bits
	ORA #0

	\\ Always add 32
	ORA #32

	\\ Rotate the pixels to scroll up
	TAX
	LDA fx_creditscroll_rotate, X

	\\ Write the byte back to the screen
	STA (writeptr), Y

	\\ Full width
	.skip
	INY
	CPY #CREDITS_last_char
	BCC x_loop

	\\ Move down a row

	LDA readptr
	STA writeptr
	LDA readptr+1
	STA writeptr+1

	CLC
	LDA readptr
	ADC #MODE7_char_width
	STA readptr
	LDA readptr+1
	ADC #0
	STA readptr+1

	\\ Check if we've reached the end?
	LDA readptr
	CMP #LO(CREDITS_end_addr)
	BNE y_loop
	LDA readptr+1
	CMP #HI(CREDITS_end_addr)
	BNE y_loop

	\\ Do last line separately

	LDY #CREDITS_first_char
	.last_loop

	\\ Mask in top pixels from our new line
	LDA fx_creditscroll_new_line, Y
	AND #&3
	STA top_bits_last+1

	\\ Load last line bottom pixels
	LDA (writeptr), Y
	AND #&FC

	\\ Merge them together
	.top_bits_last
	ORA #0

	\\ Always add 32...
	ORA #32

	\\ Rotate them
	TAX
	LDA fx_creditscroll_rotate, X

	\\ Store back to screen
	STA (writeptr), Y

	\\ Entire row
	INY
	CPY #CREDITS_last_char
	BCC last_loop

	.return
	RTS
}

.fx_creditscroll_rotate_new_line
{
	\\ First char in row
	LDY #CREDITS_first_char
	.x_loop

	\\ Get bottom pixels from current row
	LDA fx_creditscroll_new_line, Y
	AND #&FC

	ORA #32

	\\ Rotate the pixels to scroll up
	TAX
	LDA fx_creditscroll_rotate, X

	\\ Write the byte back to the screen
	STA fx_creditscroll_new_line, Y

	\\ Full width
	.skip
	INY
	CPY #CREDITS_last_char
	BCC x_loop

	.return
	RTS
}

\\ Main update function

.fx_creditscroll_update
{
	lda line_counter
	beq new_line
	dec line_counter
	rts

.new_line

	\\ Set graphics white
;	lda #144+7
;	jsr mode7_set_graphics_shadow_fast			; can remove this if other routine handling colours	

	\\ Write new line of text to array
	JSR fx_creditscroll_write_text_line

	\\ Scroll everything up
	JSR fx_creditscroll_scroll_up

	.return
	RTS
}


\ ******************************************************************
\ *	Credit Text FX
\ ******************************************************************

.fx_creditscroll_text_ptr
EQUW fx_creditscroll_text

.fx_creditscroll_text_row
EQUB 0

.fx_creditscroll_text_idx
EQUB 0

.fx_creditscroll_write_text_line
{
	\\ Write text into our new line
	LDA fx_creditscroll_text_ptr
	STA readptr
	LDA fx_creditscroll_text_ptr+1
	STA readptr+1

	LDX fx_creditscroll_text_row
	BEQ write_new_text
	CPX #3
	BEQ write_new_text

	\\ Just rotate existing line
	JSR fx_creditscroll_rotate_new_line
	JMP reached_end_of_row

	.write_new_text

	LDX #MODE7_char_width-1
	LDA #0
	.clear_loop
	STA fx_creditscroll_new_line, X
	DEX
	BPL clear_loop

	\\ Get X start
	LDY #0
	LDA (readptr), Y
	TAX

	\\ Set row

	INY
	.char_loop
	STY fx_creditscroll_text_idx

	\\ Get text char
	LDA (readptr), Y
	BNE not_end_of_string

	\\ If EOS assume EOR
	JMP reached_end_of_row

	.not_end_of_string
	;JSR fx_creditscroll_get_char		; preserves X&Y

	\\ A is index into our font data
	TAY

	.font_addr_1
	LDA mode7_font_data, Y
	INY
	STA fx_creditscroll_new_line, X

	\\ Next char cell
	INX
	CPX #MODE7_char_width
	BCS reached_end_of_row

	.font_addr_2
	LDA mode7_font_data, Y
	INY
	STA fx_creditscroll_new_line, X

	\\ Next char cell
	INX
	CPX #MODE7_char_width
	BCS reached_end_of_row

	.font_addr_3
	LDA mode7_font_data, Y
	INY
	STA fx_creditscroll_new_line, X

	\\ Next char cell
	INX
	CPX #MODE7_char_width
	BCS reached_end_of_row

	LDY fx_creditscroll_text_idx

	\\ Next text char
	INY
	JMP char_loop

	.reached_end_of_row

	\\ Next time do next row
	LDX fx_creditscroll_text_row
	INX
	CPX #3
	BNE not_three

	\\ At row 3 need to swap to next line of font data
	LDA #LO(mode7_font_data_second_row)
	STA font_addr_1+1
	STA font_addr_2+1
	STA font_addr_3+1
	LDA #HI(mode7_font_data_second_row)
	STA font_addr_1+2
	STA font_addr_2+2
	STA font_addr_3+2

	\\ There are 6 rows in total	
	.not_three
	CPX #6	
	BCC still_same_text



.new_line	

	\\ Next line of text
	LDY fx_creditscroll_text_idx

	\\ Skip to EOS
	{
		.loop
		LDA (readptr), Y
		BEQ done
		INY
		BNE loop
		.done
	}

	\\ Check whether there are any more strings
	INY
	LDA (readptr), Y
	CMP #&FF
	BNE next_line_text

	\\ Reset to start of text
	LDA #LO(fx_creditscroll_text)
	STA fx_creditscroll_text_ptr
	LDA #HI(fx_creditscroll_text)
	STA fx_creditscroll_text_ptr+1

	\\ Or just flag not to write any more text..
	JMP continue_text

	\\ Update text pointer
	.next_line_text
	TYA
	CLC
	ADC fx_creditscroll_text_ptr
	STA fx_creditscroll_text_ptr
	LDA fx_creditscroll_text_ptr+1
	ADC #0
	STA fx_creditscroll_text_ptr+1

	; insert a delay
	lda #ROW_DELAY
	sta line_counter

	\\ Next line of text
	.continue_text

	\\ Need to reset font data
	LDA #LO(mode7_font_data)
	STA font_addr_1+1
	STA font_addr_2+1
	STA font_addr_3+1
	LDA #HI(mode7_font_data)
	STA font_addr_1+2
	STA font_addr_2+2
	STA font_addr_3+2

	\\ Start from row 0
	LDX #0

	.still_same_text
	STX fx_creditscroll_text_row

	.return
	RTS
}


\ ******************************************************************
\ *	Credit Font FX
\ ******************************************************************

.fx_creditscroll_rotate
{
	FOR n, 0, 255, 1
	a = n AND 1
	b = n AND 2
	c = n AND 4
	d = n AND 8
	e = n AND 16
	f = n AND 64
	
	; Pixel pattern becomes
	;  1  2  ->  a b  ->  c d
	;  4  8  ->  c d  ->  e f 
	; 64 16  ->  e f  ->  a b

	IF (n AND 32)
	PRINT a,b,c,d,e,f
	EQUB 32 + (a * 16) + (b * 32) + (c / 4) + (d / 4) + (e / 4) + (f / 8) + (n AND 128)
	ELSE
	EQUB n
	ENDIF
	NEXT
}

\\ Spare character row which will get added to bottom of scroll
\\ Update fn so only top two pixels (1+2) get added to bottom of scroll
\\ Can rotate this row itself to shuffle new pixels onto bottom of screen

.fx_creditscroll_new_line
FOR n, 0, MODE7_char_width-1, 1
EQUB 0
NEXT


\\ Map character ASCII values to the byte offset into our MODE 7 font
\\ This is "cunning" but only works because the font has fewer than 256/6 (42) glyphs..

MACRO SET_TELETEXT_FONT_CHAR_MAP

	MAPCHAR 'A', 1
	MAPCHAR 'B', 4
	MAPCHAR 'C', 7
	MAPCHAR 'D', 10
	MAPCHAR 'E', 13
	MAPCHAR 'F', 16
	MAPCHAR 'G', 19
	MAPCHAR 'H', 22
	MAPCHAR 'I', 25
	MAPCHAR 'J', 28
	MAPCHAR 'K', 31
	MAPCHAR 'L', 34
	MAPCHAR 'M', 37

	MAPCHAR 'a', 1
	MAPCHAR 'b', 4
	MAPCHAR 'c', 7
	MAPCHAR 'd', 10
	MAPCHAR 'e', 13
	MAPCHAR 'f', 16
	MAPCHAR 'g', 19
	MAPCHAR 'h', 22
	MAPCHAR 'i', 25
	MAPCHAR 'j', 28
	MAPCHAR 'k', 31
	MAPCHAR 'l', 34
	MAPCHAR 'm', 37

	MAPCHAR 'N', 81
	MAPCHAR 'O', 84
	MAPCHAR 'P', 87
	MAPCHAR 'Q', 90
	MAPCHAR 'R', 93
	MAPCHAR 'S', 96
	MAPCHAR 'T', 99
	MAPCHAR 'U', 102
	MAPCHAR 'V', 105
	MAPCHAR 'W', 108
	MAPCHAR 'X', 111
	MAPCHAR 'Y', 114
	MAPCHAR 'Z', 117

	MAPCHAR 'n', 81
	MAPCHAR 'o', 84
	MAPCHAR 'p', 87
	MAPCHAR 'q', 90
	MAPCHAR 'r', 93
	MAPCHAR 's', 96
	MAPCHAR 't', 99
	MAPCHAR 'u', 102
	MAPCHAR 'v', 105
	MAPCHAR 'w', 108
	MAPCHAR 'x', 111
	MAPCHAR 'y', 114
	MAPCHAR 'z', 117

	MAPCHAR '0', 161
	MAPCHAR '1', 164
	MAPCHAR '2', 167
	MAPCHAR '3', 170
	MAPCHAR '4', 173
	MAPCHAR '5', 176
	MAPCHAR '6', 179
	MAPCHAR '7', 182
	MAPCHAR '8', 185
	MAPCHAR '9', 188
	MAPCHAR '?', 191
	MAPCHAR '!', 194
	MAPCHAR '.', 197

	MAPCHAR ' ', 241

ENDMACRO

SET_TELETEXT_FONT_CHAR_MAP

\\ Credit text strings
\\ First byte is character offset from left side of screen
\\ Then text string terminted by 0
\\ If character offset is &FF this indicates no more strings
\\ Currently strings just loop but could just stop!

\\ New font is 3 chars wide = max 13 letters per line from 1

.fx_creditscroll_text
;       0123456789abc
EQUS 1,"Bad Apple",0
EQUS 1,"Teletext",0
EQUS 1," ",0
EQUS 1,"A",0
EQUS 1,"Bitshifters",0
EQUS 1,"Production",0
EQUS 1," ",0
EQUS 1,"Code By",0
EQUS 1,"Kieran and",0
EQUS 1,"simondotm",0
EQUS 1," ",0
EQUS 1," ",0
EQUS 1,"Music by",0
EQUS 1,"Inverse Phase",0
EQUS 1," ",0
EQUS 1,"Art by",0
EQUS 1,"Horsenburger",0
EQUS 1," ",0
EQUS 1," ",0
EQUS 1,"Released at",0
EQUS 1,"Block Party",0
EQUS 1,"Cambridge",0
EQUS 1,"25 Feb 2017",0
EQUS 1," ",0
EQUS 1,"Greetz from",0
EQUS 1,"Kieran to...",0
EQUS 1," ",0
EQUS 1,"Raquel Meyers",0
EQUS 1,"Steve Horsley",0
EQUS 1,"Dan Farrimond",0
EQUS 1,"Simon Rawles",0
EQUS 1,"Peter KVT80",0
EQUS 1," ",0
EQUS 1,"Greetz from",0
EQUS 1,"Inverse Phase",0
EQUS 1,"to...",0
EQUS 1," ",0
EQUS 1,"",0
EQUS 1,"3LN",0
EQUS 1,"4mat",0
EQUS 1,"bitshifter",0
EQUS 1,"bitshifters",0
EQUS 1,"blargg",0
EQUS 1,"cmucc",0
EQUS 1,"crtc",0
EQUS 1,"ctrix",0
EQUS 1,"delek",0
EQUS 1,"gemini",0
EQUS 1,"goto80",0
EQUS 1,"haujobb",0
EQUS 1,"nesdev",0
EQUS 1,"pwp",0
EQUS 1,"siren",0
EQUS 1,"trixter",0
EQUS 1,"ubulab",0
EQUS 1,"virt",0
EQUS 1,"vogue",0
EQUS 1,"mr. h",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"Thankyou for",0
EQUS 1,"Watching!",0
EQUS 1," ",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS 1,"",0
EQUS &FF
.fx_creditscroll_text_end

PRINT "credits text size ", ~(fx_creditscroll_text_end-fx_creditscroll_text), " bytes"
RESET_MAPCHAR



.end_fx_creditscroll
