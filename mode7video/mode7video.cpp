// mode7video.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"
#include "CImg.h"

using namespace cimg_library;

#define MODE7_COL0			151
#define MODE7_COL1			(sep ? 154 : 32)
#define MODE7_BLANK			32
#define MODE7_WIDTH			40
#define MODE7_HEIGHT		25
#define MODE7_MAX_SIZE		(MODE7_WIDTH * MODE7_HEIGHT)

#define MODE7_BLACK_BG		156
#define MODE7_NEW_BG		157
#define MODE7_HOLD_GFX		158
#define MODE7_RELEASE_GFX	159
#define MODE7_GFX_COLOUR	144

#define JPG_W				src._width		// w				// 76
#define JPG_H				src._height		// h				// 57 = 4:3  // 42 = 16:9
#define FRAME_WIDTH			(JPG_W/2)
#define FRAME_HEIGHT		(JPG_H/3)
#define NUM_FRAMES			frames			// 5367		// 5478
#define FRAME_SIZE			(MODE7_WIDTH * FRAME_HEIGHT)

#define FILENAME			shortname
#define DIRECTORY			shortname

#define _ZERO_FRAME_PRESET	TRUE		// whether our zero frame is already setup for MODE 7

#define CLAMP(a,low,high)	((a) < (low) ? (low) : ((a) > (high) ? (high) : (a)))
#define THRESHOLD(a,t)		((a) >= (t) ? 255 : 0)
#define LO(a)				((a) % 256)
#define HI(a)				((a) / 256)

#define _COLOUR_DEBUG		FALSE

#define _USE_16_BIT_PACK	FALSE

#if _USE_16_BIT_PACK
#define BYTES_PER_DELTA		2
#else
#define BYTES_PER_DELTA		3
#endif

static CImg<unsigned char> src;
static unsigned char prevmode7[MODE7_MAX_SIZE];
static unsigned char delta[MODE7_MAX_SIZE];
static unsigned char mode7[MODE7_MAX_SIZE];

#define MAX_STATE (1U << 14)
#define GET_STATE(fg,bg,hold_mode,last_gfx_char)	((last_gfx_char) << 7 | (hold_mode) << 6 | ((bg) << 3) | (fg))

static int total_error_in_state[MAX_STATE][MODE7_WIDTH + 1];
static unsigned char char_for_xpos_in_state[MAX_STATE][MODE7_WIDTH + 1];
static unsigned char output[MODE7_WIDTH];

static bool global_use_hold = true;
static bool global_use_fill = true;

void clear_error_char_arrays(void)
{
	for (int state = 0; state < MAX_STATE; state++)
	{
		for (int x = 0; x <= MODE7_WIDTH; x++)
		{
			total_error_in_state[state][x] = -1;
			char_for_xpos_in_state[state][x] = 'X';
		}
	}
}

int get_state_for_char(unsigned char proposed_char, int old_state)
{
	int fg = old_state & 7;
	int bg = (old_state >> 3) & 7;
	int hold_mode = (old_state >> 6) & 1;
	unsigned char last_gfx_char = (old_state >> 7) & 0x7f;

	if (global_use_fill)
	{
		if (proposed_char == MODE7_NEW_BG)
		{
			bg = fg;
		}

		if (proposed_char == MODE7_BLACK_BG)
		{
			bg = 0;
		}
	}

	if (proposed_char > MODE7_GFX_COLOUR && proposed_char < MODE7_GFX_COLOUR + 8)
	{
		fg = proposed_char - MODE7_GFX_COLOUR;
	}

	if (global_use_hold)
	{
		if (proposed_char == MODE7_HOLD_GFX)
		{
			hold_mode = true;
		}

		if (proposed_char == MODE7_RELEASE_GFX)
		{
			hold_mode = false;
			last_gfx_char = MODE7_BLANK;
		}

		if (proposed_char < 128)
		{
			last_gfx_char = proposed_char;
		}
	}
	else
	{
		hold_mode = false;
		last_gfx_char = MODE7_BLANK;
	}

	return GET_STATE(fg, bg, hold_mode, last_gfx_char);
}


int get_colour_from_rgb(unsigned char r, unsigned char g, unsigned char b)
{
	return (r ? 1 : 0) + (g ? 2 : 0) + (b ? 4 : 0);
}

#define GET_RED_FROM_COLOUR(c)		(c & 1 ? 255:0)
#define GET_GREEN_FROM_COLOUR(c)	(c & 2 ? 255:0)
#define GET_BLUE_FROM_COLOUR(c)		(c & 4 ? 255:0)

unsigned char pixel_to_grey(int mode, unsigned char r, unsigned char g, unsigned char b)
{
	switch (mode)
	{
	case 1:
		return r;

	case 2:
		return g;

	case 3:
		return b;

	case 4:
		return (unsigned char)((r + g + b) / 3);

	case 5:
		return (unsigned char)(0.2126f * r + 0.7152f * g + 0.0722f * b);

	default:
		return 0;
	}
}

// For each character cell on this line
// Do we have pixels or not?
// If we have pixels then need to decide whether is it better to replace this cell with a control code or use a graphic character
// If we don't have pixels then need to decide whether it is better to insert a control code or leave empty
// Possible control codes are: new fg colour, fill (bg colour = fg colour), no fill (bg colour = black), hold graphics (hold char = prev char), release graphics (hold char = empty)
// "Better" means that the "error" for the rest of the line (appearance on screen vs actual image = deviation) is minimised

// Hold graphics mode means use last known (used on the line) graphic character in place of space when emitting a control code (reset if using alphanumerics not graphics)

// Functions - get_error_for_char(int x7, int y7, unsigned char code, int fg, int bg, unsigned char hold_char)
int get_error_for_char(int x7, int y7, unsigned char proposed_char, int fg, int bg, bool hold_mode, unsigned char last_gfx_char)
{
	int x = (x7 - (MODE7_WIDTH - FRAME_WIDTH)) * 2;
	int y = y7 * 3;
	int error = 0;

	// If proposed character >= 128 then this is a control code
	// If so then the hold char will be displayed on screen
	// Otherwise it will be our proposed character (pixels)

	unsigned char screen_char;

	if (hold_mode)
	{
		screen_char = (proposed_char >= 128) ? last_gfx_char : proposed_char;
	}
	else
	{
		screen_char = (proposed_char >= 128) ? MODE7_BLANK : proposed_char;
	}

	unsigned char screen_r, screen_g, screen_b;
	unsigned char image_r, image_g, image_b;

	// These are the pixels that will get written to the screen

	screen_r = screen_char & 1 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 1 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 1 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	// These are the pixels in the image

	image_r = src(x, y, 0);
	image_g = src(x, y, 1);
	image_b = src(x, y, 2);

	// Calculate the error between them

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	screen_r = screen_char & 2 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 2 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 2 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	image_r = src(x+1, y, 0);
	image_g = src(x+1, y, 1);
	image_b = src(x+1, y, 2);

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	screen_r = screen_char & 4 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 4 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 4 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	image_r = src(x, y+1, 0);
	image_g = src(x, y+1, 1);
	image_b = src(x, y+1, 2);

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	screen_r = screen_char & 8 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 8 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 8 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	image_r = src(x+1, y + 1, 0);
	image_g = src(x+1, y + 1, 1);
	image_b = src(x+1, y + 1, 2);

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	screen_r = screen_char & 16 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 16 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 16 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	image_r = src(x, y + 2, 0);
	image_g = src(x, y + 2, 1);
	image_b = src(x, y + 2, 2);

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	screen_r = screen_char & 64 ? GET_RED_FROM_COLOUR(fg) : GET_RED_FROM_COLOUR(bg);
	screen_g = screen_char & 64 ? GET_GREEN_FROM_COLOUR(fg) : GET_GREEN_FROM_COLOUR(bg);
	screen_b = screen_char & 64 ? GET_BLUE_FROM_COLOUR(fg) : GET_BLUE_FROM_COLOUR(bg);

	image_r = src(x + 1, y + 2, 0);
	image_g = src(x + 1, y + 2, 1);
	image_b = src(x + 1, y + 2, 2);

	error += ((screen_r - image_r) * (screen_r - image_r)) + ((screen_g - image_g) * (screen_g - image_g)) + ((screen_b - image_b) * (screen_b - image_b));

	// For all six pixels in the character cell

	return error;
}

int get_error_for_remainder_of_line(int x7, int y7, int fg, int bg, bool hold_mode, unsigned char last_gfx_char)
{
	if (x7 >= MODE7_WIDTH)
		return 0;

	int state = GET_STATE(fg, bg, hold_mode, last_gfx_char);

	if (total_error_in_state[state][x7] != -1)
		return total_error_in_state[state][x7];

//	printf("get_error_for_remainder_of_line(%d, %d, %d, %d, %d, %d)\n", x7, y7, fg, bg, hold_char, prev_char);

	unsigned char current_char = mode7[(y7 * MODE7_WIDTH) + (x7)];
	int x = (x7 - (MODE7_WIDTH - FRAME_WIDTH)) * 2;
	int y = y7 * 3;
	unsigned char graphic_char = 0;
	int lowest_error = INT_MAX;
	unsigned char lowest_char = 'Z';

	if (current_char != MODE7_BLANK)
	{
		// We have some pixels in this cell
		// Calculate graphic character - if pixel == bg colour then off else on

		graphic_char = 32 +																				// bit 5 always set!
			+(get_colour_from_rgb(src(x, y, 0), src(x, y, 1), src(x, y, 2)) == bg ? 0 : 1)						// (x,y) = bit 0
			+ (get_colour_from_rgb(src(x + 1, y, 0), src(x + 1, y, 1), src(x + 1, y, 2)) == bg ? 0 : 2)					// (x+1,y) = bit 1
			+ (get_colour_from_rgb(src(x, y + 1, 0), src(x, y + 1, 1), src(x, y + 1, 2)) == bg ? 0 : 4)					// (x,y+1) = bit 2
			+ (get_colour_from_rgb(src(x + 1, y + 1, 0), src(x + 1, y + 1, 1), src(x + 1, y + 1, 2)) == bg ? 0 : 8)			// (x+1,y+1) = bit 3
			+ (get_colour_from_rgb(src(x, y + 2, 0), src(x, y + 2, 1), src(x, y + 2, 2)) == bg ? 0 : 16)				// (x,y+2) = bit 4
			+ (get_colour_from_rgb(src(x + 1, y + 2, 0), src(x + 1, y + 2, 1), src(x + 1, y + 2, 2)) == bg ? 0 : 64);			// (x+1,y+2) = bit 6
	}
	else
	{
		graphic_char = MODE7_BLANK;
	}

	// Possible characters are: 1 + 1 + 6 + 1 + 1 + 1 + 1 = 12 possibilities x 40 columns = 12 ^ 40 combinations.  That's not going to work :)
	// Possible states for a given cell: fg=0-7, bg=0-7, hold_gfx=6 pixels : total = 12 bits = 4096 possible states
	// Wait! What about prev_char as part of state if want to use hold graphics feature? prev_char=6 pixels so actually 18 bits = 262144 possible states
	// Not all of them can be visited as we cannot arbitrarily set the previous character or hold character but still needs a 40Mb array of ints! :S

	// Graphic char (if set)
	// Stay blank (if not)
	// Set graphic colour (colour != fg) - 6
	// Fill (if bg != fg)
	// No fill (if bg != 0)
	// Hold graphics - TODO
	// Release graphics- TODO

	// If we have a graphic character or blank we could use this!
	if (graphic_char == MODE7_BLANK)
	{
		int newstate = GET_STATE(fg, bg, hold_mode, graphic_char);
		int error = get_error_for_char(x7, y7, graphic_char, fg, bg, hold_mode, graphic_char);
		int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, bg, hold_mode, graphic_char);

		if (total_error_in_state[newstate][x7 + 1] == -1)
		{
			total_error_in_state[newstate][x7 + 1] = remaining;
			char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
		}

		error += remaining;

		if (error < lowest_error)
		{
			lowest_error = error;
			lowest_char = graphic_char;
		}
	}

	// If the background is black we could enable fill! - you idiot - can enable fill at any time if fg colour has changed since last time!
	if (global_use_fill)
	{
		if (bg != fg)
		{
			// Bg colour becomes fg colour
			int newstate = GET_STATE(fg, fg, hold_mode, last_gfx_char);
			int error = get_error_for_char(x7, y7, MODE7_NEW_BG, fg, fg, hold_mode, last_gfx_char);
			int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, fg, hold_mode, last_gfx_char);

			if (total_error_in_state[newstate][x7 + 1] == -1)
			{
				total_error_in_state[newstate][x7 + 1] = remaining;
				char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
			}

			error += remaining;

			if (error < lowest_error)
			{
				lowest_error = error;
				lowest_char = MODE7_NEW_BG;
			}
		}

		// If the background is not black we could disable fill!
		if (bg != 0)
		{
			// Bg colour becomes black
			int newstate = GET_STATE(fg, 0, hold_mode, last_gfx_char);
			int error = get_error_for_char(x7, y7, MODE7_BLACK_BG, fg, 0, hold_mode, last_gfx_char);
			int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, 0, hold_mode, last_gfx_char);

			if (total_error_in_state[newstate][x7 + 1] == -1)
			{
				total_error_in_state[newstate][x7 + 1] = remaining;
				char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
			}

			error += remaining;

			if (error < lowest_error)
			{
				lowest_error = error;
				lowest_char = MODE7_BLACK_BG;
			}
		}
	}

	// We could enter hold graphics mode!
	if (global_use_hold)
	{
		if (!hold_mode)
		{
			int newstate = GET_STATE(fg, bg, true, last_gfx_char);
			int error = get_error_for_char(x7, y7, MODE7_HOLD_GFX, fg, bg, true, last_gfx_char);
			int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, bg, true, last_gfx_char);

			if (total_error_in_state[newstate][x7 + 1] == -1)
			{
				total_error_in_state[newstate][x7 + 1] = remaining;
				char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
			}

			error += remaining;

			if (error < lowest_error)
			{
				lowest_error = error;
				lowest_char = MODE7_HOLD_GFX;
			}
		}

		// We could exit hold graphics mode..
		else
		{
			int newstate = GET_STATE(fg, bg, false, MODE7_BLANK);
			int error = get_error_for_char(x7, y7, MODE7_RELEASE_GFX, fg, bg, false, MODE7_BLANK);
			int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, bg, false, MODE7_BLANK);

			if (total_error_in_state[newstate][x7 + 1] == -1)
			{
				total_error_in_state[newstate][x7 + 1] = remaining;
				char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
			}

			error += remaining;

			if (error < lowest_error)
			{
				lowest_error = error;
				lowest_char = MODE7_RELEASE_GFX;
			}
		}
	}

	for (int c = 1; c < 8; c++)
	{
		// We could change our fg colour!
		if (c != fg)
		{
			int newstate = GET_STATE(c, bg, hold_mode, last_gfx_char);

			int error = get_error_for_char(x7, y7, MODE7_GFX_COLOUR + c, c, bg, hold_mode, last_gfx_char);

			int remaining = get_error_for_remainder_of_line(x7 + 1, y7, c, bg, hold_mode, last_gfx_char);

			if (total_error_in_state[newstate][x7 + 1] == -1)
			{
				total_error_in_state[newstate][x7 + 1] = remaining;
				char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
			}

			error += remaining;

			if (error < lowest_error)
			{
				lowest_error = error;
				lowest_char = MODE7_GFX_COLOUR + c;
			}
		}
	}

	if (graphic_char != MODE7_BLANK)
	{
		int newstate = GET_STATE(fg, bg, hold_mode, global_use_hold ? graphic_char : MODE7_BLANK);
		int error = get_error_for_char(x7, y7, graphic_char, fg, bg, hold_mode, global_use_hold ? graphic_char : MODE7_BLANK);
		int remaining = get_error_for_remainder_of_line(x7 + 1, y7, fg, bg, hold_mode, global_use_hold ? graphic_char : MODE7_BLANK);

		if (total_error_in_state[newstate][x7 + 1] == -1)
		{
			total_error_in_state[newstate][x7 + 1] = remaining;
			char_for_xpos_in_state[newstate][x7 + 1] = output[x7 + 1];
		}

		error += remaining;

		if (error < lowest_error)
		{
			lowest_error = error;
			lowest_char = graphic_char;
		}
	}

//	printf("(%d, %d) returning char=%d lowest error=%d\n", x7, y7, lowest_char, lowest_error);

	output[x7] = lowest_char;

	return lowest_error;
}


int main(int argc, char **argv)
{
	cimg_usage("MODE 7 video convertor.\n\nUsage : mode7video [options]");
	const int frames = cimg_option("-n", 0, "Last frame number");
	const int start = cimg_option("-s", 1, "Start frame number");
	const char *const shortname = cimg_option("-i", (char*)0, "Input (directory / short name)");
	const char *const ext = cimg_option("-e", (char*)"png", "Image format file extension");
	const int gmode = cimg_option("-g", 0, "Colour to greyscale conversion (0=none, 1=red only, 2=green only, 3=blue only, 4=simple average, 5=luminence preserving");
	const int thresh = cimg_option("-t", 127, "B&W threshold value");
	const int dither = cimg_option("-d", 0, "Dither mode (0=none/threshold only, 1=floyd steinberg, 2=ordered 2x2, 3=ordered 3x3");
	const bool no_hold = cimg_option("-nohold", false, "Disallow Hold Graphics control code");
	const bool no_fill = cimg_option("-nofill", false, "Disallow New Background control code");
	const bool save = cimg_option("-save", false, "Save individual MODE7 frames");
	const bool simg = cimg_option("-simg", false, "Save individual image frames");
	const bool sep = cimg_option("-sep", false, "Separated graphics");
	const bool verbose = cimg_option("-v", false, "Verbose output");

	if (cimg_option("-h", false, 0)) std::exit(0);
	if (shortname == NULL)  std::exit(0);

	global_use_hold = !no_hold;
	global_use_fill = !no_fill;

	char filename[256];
	char input[256];

	int totaldeltas = 0;
	int totalbytes = 0;
	int maxdeltas = 0;
	int resetframes = 0;

	unsigned char *beeb = (unsigned char *) malloc(MODE7_MAX_SIZE * NUM_FRAMES);
	unsigned char *ptr = beeb;

	int *delta_counts = (int *)malloc(sizeof(int) * (NUM_FRAMES+1));

	memset(mode7, 0, MODE7_MAX_SIZE);
	memset(prevmode7, 0, MODE7_MAX_SIZE);
	memset(delta, 0, MODE7_MAX_SIZE);
	memset(delta_counts, 0, sizeof(int) * (NUM_FRAMES+1));

	FILE *file;

	// Blank MODE 7 gfx screen

	for (int i = 0; i < MODE7_MAX_SIZE; i++)
	{
#if _ZERO_FRAME_PRESET
		switch (i % MODE7_WIDTH)
		{
		case 0:
			prevmode7[i] = MODE7_COL0;
			break;

		case 1:
			prevmode7[i] = MODE7_COL1;
			break;

		default:
			prevmode7[i] = MODE7_BLANK;
			break;
		}
#else
		prevmode7[i] = MODE7_BLANK;
#endif
	}

	for (int n = start; n <= NUM_FRAMES; n++)
	{
		sprintf(input, "%s\\frames\\%s-%d.%s", DIRECTORY, FILENAME, n, ext);
		src.assign(input);

		// Convert to greyscale from RGB

		if (gmode)
		{
			cimg_forXY(src, x, y)
			{
				src(x, y, 0) = pixel_to_grey(gmode, src(x, y, 0), src(x, y, 1), src(x, y, 2));
				src(x, y, 1) = pixel_to_grey(gmode, src(x, y, 0), src(x, y, 1), src(x, y, 2));
				src(x, y, 2) = pixel_to_grey(gmode, src(x, y, 0), src(x, y, 1), src(x, y, 2));
			}
		}

		// Dithering

		if (dither == 0)					// None / threshold
		{
			cimg_forXY(src, x, y)
			{
				src(x, y, 0) = THRESHOLD(src(x, y, 0), thresh);
				src(x, y, 1) = THRESHOLD(src(x, y, 1), thresh);
				src(x, y, 2) = THRESHOLD(src(x, y, 2), thresh);
			}
		}
		else if (dither == 1)				// Floyd Steinberg
		{
			cimg_forXY(src, x, y)
			{
				int grey = src(x, y, 0);
				src(x, y, 0) = THRESHOLD(grey, 128);
				int error = grey - src(x, y, 0);

				if (x < src._width - 1)
				{
					grey = src(x + 1, y, 0) + error * 7 / 16;
					src(x + 1, y, 0) = CLAMP(grey, 0, 255);
				}

				if( y < src._height - 1 )
				{
					if (x > 0)
					{
						grey = src(x - 1, y + 1, 0) + error * 3 / 16;
						src(x - 1, y + 1, 0) = CLAMP(grey, 0, 255);
					}

					grey = src(x, y + 1, 0) + error * 5 / 16;
					src(x, y + 1, 0) = CLAMP(grey, 0, 255);

					if (x < src._width - 1)
					{
						grey = src(x + 1, y + 1, 0) + error * 1 / 16;
						src(x + 1, y + 1, 0) = CLAMP(grey, 0, 255);
					}
						
				}
			}
		}
		else if (dither == 2)						// Ordered dither 2x2
		{
			cimg_forY(src, y)
			{
				cimg_forX(src, x)
				{
					int grey = src(x, y, 0) * 5 / 256;
					src(x, y, 0) = THRESHOLD(grey, 1);

					if (x < src._width - 1)
					{
						grey = src(x + 1, y, 0) * 5 / 256;
						src(x + 1, y, 0) = THRESHOLD(grey, 3);
					}

					if (y < src._height - 1)
					{
						grey = src(x, y + 1, 0) * 5 / 256;
						src(x, y + 1, 0) = THRESHOLD(grey, 4);

						if (x < src._width - 1)
						{
							grey = src(x + 1, y + 1, 0) * 5 / 256;
							src(x + 1, y + 1, 0) = THRESHOLD(grey, 2);
						}
					}
					x++;
				}
				y++;
			}
		}
		else if (dither == 3)						// Ordered dither 3x3
		{
			cimg_forY(src, y)
			{
				cimg_forX(src, x)
				{
					int grey = src(x, y, 0) * 10 / 256;
					src(x, y, 0) = THRESHOLD(grey, 1);

					if (x < src._width - 1)
					{
						grey = src(x + 1, y, 0) * 10 / 256;
						src(x + 1, y, 0) = THRESHOLD(grey, 8);
					}

					if (x < src._width - 2)
					{
						grey = src(x + 2, y, 0) * 10 / 256;
						src(x + 2, y, 0) = THRESHOLD(grey, 4);
					}

					if (y < src._height - 1)
					{
						grey = src(x, y + 1, 0) * 10 / 256;
						src(x, y + 1, 0) = THRESHOLD(grey, 7);

						if (x < src._width - 1)
						{
							grey = src(x + 1, y + 1, 0) * 10 / 256;
							src(x + 1, y + 1, 0) = THRESHOLD(grey, 6);
						}

						if (x < src._width - 2)
						{
							grey = src(x + 2, y + 1, 0) * 10 / 256;
							src(x + 2, y + 1, 0) = THRESHOLD(grey, 3);
						}
					}

					if (y < src._height - 2)
					{
						grey = src(x, y + 2, 0) * 10 / 256;
						src(x, y + 2, 0) = THRESHOLD(grey, 5);

						if (x < src._width - 1)
						{
							grey = src(x + 1, y + 2, 0) * 10 / 256;
							src(x + 1, y + 2, 0) = THRESHOLD(grey, 2);
						}

						if (x < src._width - 2)
						{
							grey = src(x + 2, y + 2, 0) * 10 / 256;
							src(x + 2, y + 2, 0) = THRESHOLD(grey, 9);
						}
					}

					x+=2;
				}

				y+=2;
			}
		}

		if (simg)
		{
			sprintf(filename, "%s\\test\\%s-%d.png", DIRECTORY, FILENAME, n);
			src.save(filename);
		}

		// Conversion to MODE 7

		cimg_forY(src, y)
		{
			int y7 = y / 3;
			mode7[y7 * MODE7_WIDTH] = MODE7_COL0; // graphic white
			mode7[1 + (y7 * MODE7_WIDTH)] = MODE7_COL1; // graphic white

			int colour_counts_per_char[MODE7_WIDTH][8];
			int fg_bg_per_char[MODE7_WIDTH][2];

			for (int x7 = 0; x7 < MODE7_WIDTH; x7++)
			{
				for (int c = 0; c < 8; c++)
					colour_counts_per_char[x7][c] = 0;

				fg_bg_per_char[x7][0] = 0;
				fg_bg_per_char[x7][1] = 0;		// should it be 7 for white?
			}

			cimg_forX(src, x)
			{
				int x7 = (x / 2) + (MODE7_WIDTH - FRAME_WIDTH);

				colour_counts_per_char[x7][get_colour_from_rgb(src(x, y, 0), src(x, y, 1), src(x, y, 2))]++;
				colour_counts_per_char[x7][get_colour_from_rgb(src(x+1, y, 0), src(x+1, y, 1), src(x+1, y, 2))]++;
				colour_counts_per_char[x7][get_colour_from_rgb(src(x, y+1, 0), src(x, y+1, 1), src(x, y+1, 2))]++;
				colour_counts_per_char[x7][get_colour_from_rgb(src(x+1, y+1, 0), src(x+1, y+1, 1), src(x+1, y+1, 2))]++;
				colour_counts_per_char[x7][get_colour_from_rgb(src(x, y+2, 0), src(x, y+2, 1), src(x, y+2, 2))]++;
				colour_counts_per_char[x7][get_colour_from_rgb(src(x+1, y+2, 0), src(x+1, y+2, 1), src(x+1, y+2, 2))]++;

			//	printf("(%d, %d) = (0x%x, 0x%x, 0x%x)\n", x, y, src(x, y, 0), src(x, y, 1), src(x, y, 2));

			//	mode7[(y7 * MODE7_WIDTH) + (x7)] = 32															// bit 5 always set!
			//			+ (src(x, y, 0)				?  1 : 0)			// (x,y) = bit 0
			//			+ (src(x + 1, y, 0)			?  2 : 0)			// (x+1,y) = bit 1
			//			+ (src(x, y + 1, 0)			?  4 : 0)			// (x,y+1) = bit 2
			//			+ (src(x + 1, y + 1, 0)		?  8 : 0)			// (x+1,y+1) = bit 3
			//			+ (src(x, y + 2, 0)			? 16 : 0)			// (x,y+2) = bit 4
			//			+ (src(x + 1, y + 2, 0)		? 64 : 0);			// (x+1,y+2) = bit 6

				// Just indicate pixels or not to speed up MODE 7 conversion
				mode7[(y7 * MODE7_WIDTH) + (x7)] = colour_counts_per_char[x7][0] == 6 ? 32 : 255;

				x++;

				// Do some colour counting on this line (not actually nececssary)

				int unique_colours = 0;
				int dominant_colour = 0;
				int max_count = 0;

				for (int c = 1; c < 8; c++)
				{
					if (colour_counts_per_char[x7][c]) unique_colours++;
					if (colour_counts_per_char[x7][c] && colour_counts_per_char[x7][c] >= max_count)			// must have a count to count!
					{
						max_count = colour_counts_per_char[x7][c];
						dominant_colour = c;
					}
				}

				if (colour_counts_per_char[x7][0])				// black always a background colour
				{
					fg_bg_per_char[x7][0] = 0;
					fg_bg_per_char[x7][1] = dominant_colour;
				}
				else
				{
					fg_bg_per_char[x7][0] = dominant_colour;

					max_count = 0;
					dominant_colour = 0;

					for (int c = 1; c < 8; c++)
					{
						if ( c!= fg_bg_per_char[x7][0] && colour_counts_per_char[x7][c] && colour_counts_per_char[x7][c] >= max_count)			// must have a count to count!
						{
							max_count = colour_counts_per_char[x7][c];
							dominant_colour = c;
						}
					}

					fg_bg_per_char[x7][1] = dominant_colour;
				}

#if _COLOUR_DEBUG
				printf("(%d, %d) = [%d %d %d %d %d %d %d %d] (u=%d bg=%d fg=%d)\n", x7, y7, colour_counts_per_char[x7][0], colour_counts_per_char[x7][1], colour_counts_per_char[x7][2], colour_counts_per_char[x7][3], colour_counts_per_char[x7][4], colour_counts_per_char[x7][5], colour_counts_per_char[x7][6], colour_counts_per_char[x7][7], unique_colours, fg_bg_per_char[x7][0], fg_bg_per_char[x7][1]);
#endif
			}

			// Here we have:
			// pixel data turned into graphic characters in mode7 array - actually just an indication if pixels in cell
			// counts of each colour in a character cell - not useful?
			// proposed fg and bg colour per character cell - could be used as start of line control code?

			// Reset state as starting new character row
			// State = fg colour + bg colour + hold character + prev character
			// For each character cell on this line
			// Do we have pixels or not?
			// If we have pixels then need to decide whether is it better to replace this cell with a control code or keep pixels
			// If we don't have pixels then need to decide whether it is better to insert a control code or leave empty
			// Possible control codes are: new fg colour, fill (bg colour = fg colour), no fill (bg colour = black), hold graphics (hold char = prev char), release graphics (hold char = empty)
			// "Better" means that the "error" for the rest of the line (appearance on screen vs actual image = deviation) is minimised

			// Clear our array of error values for each state & x position
			clear_error_char_arrays();

			// This is our initial state of the screen
			int state = GET_STATE(7, 0, false, MODE7_BLANK);

			// Kick off recursive error calculation with that state
			int error = get_error_for_remainder_of_line((MODE7_WIDTH - FRAME_WIDTH), y7, 7, 0, false, MODE7_BLANK);
			char_for_xpos_in_state[state][(MODE7_WIDTH - FRAME_WIDTH)] = output[(MODE7_WIDTH - FRAME_WIDTH)];

			// Copy the resulting character data into MODE 7 screen
			for (int x7 = (MODE7_WIDTH - FRAME_WIDTH); x7 < MODE7_WIDTH; x7++)
			{
				// Copy character chosen in this position for this state
				unsigned char best_char = char_for_xpos_in_state[state][x7];

				mode7[(y7 * MODE7_WIDTH) + (x7)] = best_char;

				// Update the state
				state = get_state_for_char(best_char, state);
			}

			// printf("\n");

			y += 2;
		}

//		for (int i = 0; i < 1000; i++)
//		{
//			printf("0x%x ", mode7[i]);
//			if (i % 40 == 39)printf("\n");
//		}

		if (n == start)
		{
			*ptr++ = LO(FRAME_SIZE);
			*ptr++ = HI(FRAME_SIZE);

			totalbytes += 2;
		}

		// How many deltas?
		int numdeltas = 0;
		int numdeltabytes = 0;
		int numliterals = 0;
		int numlitbytes = 0;

		for (int i = 0; i < FRAME_SIZE; i++)
		{
			if (mode7[i] == prevmode7[i])
			{
				delta[i] = 0;
			}
			else
			{
			//	printf("N=%d mode7[%d]=%x prev[%d]=%x\n", n, i, mode7[i], i, prevmode7[i]);
				delta[i] = mode7[i];
				numdeltas++;
			}
		}

		totaldeltas += numdeltas;
		if (numdeltas > maxdeltas) maxdeltas = numdeltas;
		delta_counts[n] = numdeltas;

		if (numdeltas > FRAME_SIZE/BYTES_PER_DELTA)
		{
			numdeltabytes = FRAME_SIZE;
			resetframes++;

			if (verbose)
			{
				printf("*** RESET *** (%x)\n", ptr - beeb);
			}

			*ptr++ = 0;
			*ptr++ = 0xff;

			memcpy(ptr, mode7, FRAME_SIZE);
			ptr += FRAME_SIZE;
		}
		else
		{
			numdeltabytes = numdeltas * BYTES_PER_DELTA;

			*ptr++ = LO(numdeltas);
			*ptr++ = HI(numdeltas);

			int previ = 0;

			for (int i = 0; i < FRAME_SIZE; i++)
			{
				if (delta[i] != 0)
				{
					unsigned char byte = mode7[i];			//  ^ prevmode7[i] for EOR with prev.
#if _USE_16_BIT_PACK

					unsigned short pack = byte & 31;		// remove bits 5 & 6

					pack |= (byte & 64) >> 1;				// shift bit 6 down
					pack = (i - previ) + (pack << 10);						// shift whole thing up 10 bits and add offset

					*ptr++ = LO(pack);
					*ptr++ = HI(pack);
#else
					// No pack
					*ptr++ = LO((i - previ));
					*ptr++ = HI((i - previ));
					*ptr++ = byte;
#endif
					previ = i;								// or 0 for offset from screen start
				}
			}
		}

		{
			int blanks = 0;

			for (int i = 0; i < FRAME_SIZE; i++)
			{
				if (delta[i] == 0 && blanks<255)
				{
					blanks++;
				}
				else
				{
					int m = i;
					while (delta[m] != 0 && m < FRAME_SIZE) m++;
					int literals = m - i;
					
					blanks = 0;

					numliterals++;

					numlitbytes += 2 + literals;

					i = m;

					// Terminate early if last literal
					while (delta[m] == 0 && m < FRAME_SIZE) m++;
					if (m == FRAME_SIZE) i = m;
				}
			}
		}

		if (verbose)
		{
			printf("Frame: %d  numdeltas=%d (%d) numliterals=%d (%d)\n", n, numdeltas, numdeltabytes, numliterals, numlitbytes);
		}
		else
		{
			printf("\rFrame: %d/%d", n, NUM_FRAMES);
		}

		totalbytes += 2 + numdeltabytes;

		if (save)
		{
			sprintf(filename, "%s\\bin\\%s-%d.bin", DIRECTORY, FILENAME, n);
			file = fopen((const char*)filename, "w");

			if (file)
			{
				fwrite(mode7, 1, FRAME_SIZE, file);
				fclose(file);
			}

			sprintf(filename, "%s\\delta\\%s-%d.delta.bin", DIRECTORY, FILENAME, n);
			file = fopen((const char*)filename, "w");

			if (file)
			{
				fwrite(delta, 1, FRAME_SIZE, file);
				fclose(file);
			}
			/*
			sprintf(filename, "%s\\inf\\%s-%d.bin.inf", DIRECTORY, FILENAME, n);
			file = fopen((const char*)filename, "wb");

			if (file)
			{
				sprintf(buffer, "$.BAD%d\tFF7C00 FF7C00\n", n);
				fwrite(buffer, 1, strlen(buffer), file);
				fclose(file);
			}
			*/
		}

		memcpy(prevmode7, mode7, MODE7_MAX_SIZE);

	}

	*ptr++ = 0xff;
	*ptr++ = 0xff;

	int total_frames = NUM_FRAMES - start + 1;
	printf("\ntotal frames = %d\n", total_frames);
	printf("frame size = %d\n", FRAME_SIZE);
	printf("total deltas = %d\n", totaldeltas);
	printf("total bytes = %d\n", totalbytes);
	printf("max deltas = %d\n", maxdeltas);
	printf("reset frames = %d\n", resetframes);
	printf("deltas / frame = %f\n", totaldeltas / (float)total_frames);
	printf("bytes / frame = %f\n", totalbytes / (float)total_frames);
	printf("bytes / second = %f\n", 25.0f * totalbytes / (float)total_frames);
	printf("beeb size = %d bytes\n", ptr - beeb);

	sprintf(filename, "%s\\%s_beeb.bin", DIRECTORY, FILENAME);
	file = fopen((const char*)filename, "wb");

	if (file)
	{
		fwrite(beeb, 1, ptr-beeb, file);
		fclose(file);
	}

	free(beeb);
	free(delta_counts);

    return 0;
}

