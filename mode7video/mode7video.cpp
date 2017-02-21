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

#define FRAME_FIRST_COLUMN	(MODE7_WIDTH - FRAME_WIDTH)

#define FILENAME			shortname
#define DIRECTORY			shortname

#define _ZERO_FRAME_PRESET	TRUE		// whether our zero frame is already setup for MODE 7

#define CLAMP(a,low,high)	((a) < (low) ? (low) : ((a) > (high) ? (high) : (a)))
#define THRESHOLD(a,t)		((a) >= (t) ? 255 : 0)
#define LO(a)				((a) % 256)
#define HI(a)				((a) / 256)

#define BYTES_PER_DELTA		2

static CImg<unsigned char> src;
static unsigned char prevmode7[MODE7_MAX_SIZE];
static unsigned char delta[MODE7_MAX_SIZE];
static unsigned char mode7[MODE7_MAX_SIZE];


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

unsigned char get_graphic_char_from_image(int x7, int y7, int fg, int bg)
{
	int x = (x7 - FRAME_FIRST_COLUMN) * 2;
	int y = y7 * 3;
	unsigned char graphic_char;

		// We have some pixels in this cell
		// Calculate graphic character - if pixel == bg colour then off else on

		graphic_char = 32 +																				// bit 5 always set!
			+(get_colour_from_rgb(src(x, y, 0), src(x, y, 1), src(x, y, 2)) == bg ? 0 : 1)						// (x,y) = bit 0
			+ (get_colour_from_rgb(src(x + 1, y, 0), src(x + 1, y, 1), src(x + 1, y, 2)) == bg ? 0 : 2)					// (x+1,y) = bit 1
			+ (get_colour_from_rgb(src(x, y + 1, 0), src(x, y + 1, 1), src(x, y + 1, 2)) == bg ? 0 : 4)					// (x,y+1) = bit 2
			+ (get_colour_from_rgb(src(x + 1, y + 1, 0), src(x + 1, y + 1, 1), src(x + 1, y + 1, 2)) == bg ? 0 : 8)			// (x+1,y+1) = bit 3
			+ (get_colour_from_rgb(src(x, y + 2, 0), src(x, y + 2, 1), src(x, y + 2, 2)) == bg ? 0 : 16)				// (x,y+2) = bit 4
			+ (get_colour_from_rgb(src(x + 1, y + 2, 0), src(x + 1, y + 2, 1), src(x + 1, y + 2, 2)) == bg ? 0 : 64);			// (x+1,y+2) = bit 6

	return graphic_char;
}

int flushcode(unsigned char curcode, int curcount, unsigned char **p)
{
	switch (curcode)
	{
	case MODE7_BLANK:
		if (p)
		{
			*(*p)++ = 0 + curcount;
		//	printf("0x%02x ", curcount);
		}
		// write 0+curcount as byte
		return 1;

	case 127:			// block
		if (p)
		{
			*(*p)++ = 64 + curcount;
		//	printf("0x%02x ", 64+curcount);
		}
			// write 64+curcount as byte
		return 1;

	default:
		break;
	}

	return 0;
}

int calc_steve_size(unsigned char *screen, unsigned char blank, unsigned char **ptrtoptr)
{
	int numbytes = 0;

	for (int y7 = 0; y7 < FRAME_HEIGHT; y7++)
	{
		// Steve encode

		int x7 = FRAME_FIRST_COLUMN;
		unsigned char curcode = 0;
		int curcount = 0;

		while (x7 < MODE7_WIDTH)
		{
			unsigned char curchar = screen[y7*MODE7_WIDTH + x7];

			if (curchar == blank)
			{
				if (curcode == MODE7_BLANK)
				{
					curcount++;
				}
				else
				{
					numbytes += flushcode(curcode, curcount, ptrtoptr);
					curcode = MODE7_BLANK;
					curcount = 1;
				}
			}
			else if (curchar == 127)
			{
				if (curcode == 127)
				{
					curcount++;
				}
				else
				{
					numbytes += flushcode(curcode, curcount, ptrtoptr);
					curcode = 127;
					curcount = 1;
				}
			}
			else
			{
				numbytes += flushcode(curcode, curcount, ptrtoptr);
				if (ptrtoptr)
				{
					*(*ptrtoptr)++ = curchar | 128;
				//	printf("0x%02x ", curchar | 128);
				}
				numbytes += 1;			// graphic char
				curcode = curcount = 0;
			}

			x7++;
		}

		numbytes += flushcode(curcode, curcount, ptrtoptr);
	}

	if (ptrtoptr)
	{
	//	printf("\n");
	}

	return numbytes;
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
	const bool save = cimg_option("-save", false, "Save individual MODE7 frames");
	const bool simg = cimg_option("-simg", false, "Save individual image frames");
	const bool sep = cimg_option("-sep", false, "Separated graphics");
	const bool verbose = cimg_option("-v", false, "Verbose output");

	if (cimg_option("-h", false, 0)) std::exit(0);
	if (shortname == NULL)  std::exit(0);

	char filename[256];
	char input[256];

	int totaldeltas = 0;
	int totalbytes = 0;
	int maxdeltas = 0;
	int resetframes = 0;
	int numpads = 0;
	int totalsteve = 0;
	int totalsteved = 0;
	int totalmin = 0;

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

		cimg_forXY(src, x, y)
		{
			src(x, y, 0) = THRESHOLD(src(x, y, 0), thresh);
			src(x, y, 1) = THRESHOLD(src(x, y, 1), thresh);
			src(x, y, 2) = THRESHOLD(src(x, y, 2), thresh);
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


			// Copy the resulting character data into MODE 7 screen
			for (int x7 = FRAME_FIRST_COLUMN; x7 < MODE7_WIDTH; x7++)
			{
				mode7[(y7 * MODE7_WIDTH) + (x7)] = get_graphic_char_from_image(x7, y7, 7, 0);
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
		numdeltabytes = numdeltas * BYTES_PER_DELTA;

		int stevebytes = calc_steve_size(mode7, MODE7_BLANK, NULL);
		int stevedbytes = INT_MAX;// calc_steve_size(delta, 0, NULL);
		int minsteve = stevebytes < stevedbytes ? stevebytes : stevedbytes;

		if (numdeltabytes < minsteve)
		{
			if (numdeltas == 0)
			{
				// Blank frame
				*ptr++ = 0;
			}
			else
			{
				// Delta frame

				if (numdeltas > FRAME_SIZE / BYTES_PER_DELTA)
				{
					printf("*** RESET *** (%x)\n", ptr - beeb);
				}

				if (numdeltas > 0xFC)
				{
					printf("*** OVERFLOW *** (%x)\n", ptr - beeb);
				}

#if 0
				*ptr++ = 1;

				int previ = 0;

				for (int i = 0; i < FRAME_SIZE; i++)
				{
					if (delta[i] != 0)
					{
						unsigned char byte = mode7[i];			//  ^ prevmode7[i] for EOR with prev.

						int offset = (i - previ);

						if (previ == 0)
						{
							*ptr++ = HI(offset);				// offset HI
							offset -= HI(offset) * 256;			// special case	- THIS CAN RESULT IN VALID ZERO OFFSET
						}

						while (offset > 255)
						{
							*ptr++ = 0xff;						// max offset
							*ptr++ = 0;							// no char

							offset -= 255;
							numpads++;
						}

						*ptr++ = offset;
						*ptr++ = byte;

						previ = i;								// or 0 for offset from screen start
					}
				}

				*ptr++ = 0;					// end of frame offset
				*ptr++ = 0xff;				// end of frame byte		do we need this?
#else
				*ptr++ = LO(numdeltas);

				int previ = 0;

				for (int i = 0; i < FRAME_SIZE; i++)
				{
					if (delta[i] != 0)
					{
						unsigned char byte = mode7[i];			//  ^ prevmode7[i] for EOR with prev.

						int offset = (i - previ);
						int data = (byte & 31) | ((byte & 64) >> 1);		// mask out bit 5, shift down bit 6

						unsigned short pack = (data << 10) | offset;

						*ptr++ = LO(pack);
						*ptr++ = HI(pack);

						previ = i;								// or 0 for offset from screen start
					}
				}
#endif
			}
		}
		else
		{
			// Steve frame

			if (stevebytes < stevedbytes)
			{
				// Full steve
				*ptr++ = 0xFE;

				calc_steve_size(mode7, MODE7_BLANK, &ptr);
			}
			else
			{
				// Delta steve
				*ptr++ = 0XFD;

				calc_steve_size(delta, 0, &ptr);
			}

		}


		if (verbose)
		{
			printf("Frame: %d  numdeltas=%d (%d) stevebytes=%d stevedbytes=%d\n", n, numdeltas, numdeltabytes, stevebytes, stevedbytes);
		}
		else
		{
			printf("\rFrame: %d/%d", n, NUM_FRAMES);
		}

		totalmin += 2 + (numdeltabytes < minsteve ? numdeltabytes : minsteve);

		totalbytes += 2 + numdeltabytes;
		totalsteve += stevebytes;
		totalsteved += stevedbytes;

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

		//		if (n % 10 == 0) n++;
	}

	*ptr++ = 0xff;					// end of stream
//	*ptr++ = 0xff;

	int total_frames = NUM_FRAMES - start + 1;
	printf("\ntotal frames = %d\n", total_frames);
	printf("frame size = %d\n", FRAME_SIZE);
	printf("total deltas = %d\n", totaldeltas);
	printf("total bytes = %d\n", totalbytes);
	printf("max deltas = %d\n", maxdeltas);
	printf("reset frames = %d\n", resetframes);
	printf("pad deltas = %d\n", numpads);
	printf("actual data size = %d\n", (ptr - beeb));
	printf("deltas / frame = %f\n", totaldeltas / (float)total_frames);
	printf("bytes / frame = %f\n", totalbytes / (float)total_frames);
	printf("bytes / second = %f\n", 25.0f * totalbytes / (float)total_frames);
	printf("beeb size = %d bytes\n", ptr - beeb);
	printf("steve byte size = %d\n", totalsteve);
	printf("steve delta byte size = %d\n", totalsteved);
	printf("theoretical minimum = %d", totalmin);

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

