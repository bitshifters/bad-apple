// mode7video.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"
#include "CImg.h"

using namespace cimg_library;

#define MODE7_COL0		151
#define MODE7_COL1		(sep ? 154 : 32)
#define MODE7_BLANK		32
#define MODE7_WIDTH		40
#define MODE7_HEIGHT	25
#define MODE7_MAX_SIZE	(MODE7_WIDTH * MODE7_HEIGHT)

#define JPG_W			src._width		// w				// 76
#define JPG_H			src._height		// h				// 57 = 4:3  // 42 = 16:9
#define FRAME_WIDTH		(JPG_W/2)
#define FRAME_HEIGHT	(JPG_H/3)
#define NUM_FRAMES		frames			// 5367		// 5478
#define FRAME_SIZE		(MODE7_WIDTH * FRAME_HEIGHT)

#define FILENAME		shortname		// "bad"	// "grav"
#define DIRECTORY		shortname		// "bad"	// "grav"

#define _ZERO_FRAME_PRESET TRUE		// whether our zero frame is already setup for MODE 7

#define CLAMP(a,low,high)		((a) < (low) ? (low) : ((a) > (high) ? (high) : (a)))
#define THRESHOLD(a,t)			((a) >= (t) ? 255 : 0)
#define LO(a)					((a) % 256)
#define HI(a)					((a) / 256)

#define _COLOUR_DEBUG		TRUE

static CImg<unsigned char> src;

int get_colour_from_rgb(unsigned char r, unsigned char g, unsigned char b)
{
	return (r ? 1 : 0) + (g ? 2 : 0) + (b ? 4 : 0);
}

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

int main(int argc, char **argv)
{
	cimg_usage("MODE 7 video convertor.\n\nUsage : mode7video [options]");
//	const char *const geom = cimg_option("-g", "76x57", "Input size (ignored for now)");
	const int frames = cimg_option("-n", 0, "Last frame number");
	const int start = cimg_option("-s", 1, "Start frame number");
	const char *const shortname = cimg_option("-i", (char*)0, "Input (directory / short name)");
	const char *const ext = cimg_option("-e", (char*)"png", "Image format file extension");
	const int gmode = cimg_option("-g", 0, "Colour to greyscale conversion (0=none, 1=red only, 2=green only, 3=blue only, 4=simple average, 5=luminence preserving");
	const int thresh = cimg_option("-t", 127, "B&W threshold value");
	const int dither = cimg_option("-d", 0, "Dither mode (0=none/threshold only, 1=floyd steinberg, 2=ordered 2x2, 3=ordered 3x3");
	const bool save = cimg_option("-save", false, "Save individual MODE7 frames");
	const bool simg = cimg_option("-simg", false, "Save individual image frames");
	const bool sep = cimg_option("-sep", false, "Separated graphics");
	const bool verbose = cimg_option("-v", false, "Verbose output");
//	const int cbr_frames = cimg_option("-cbr", 0, "CBR frames [experimental/unfinished]");

//	int w = 76, h = 57;
//	std::sscanf(geom, "%d%*c%d", &w, &h);

	if (cimg_option("-h", false, 0)) std::exit(0);
	if (shortname == NULL)  std::exit(0);

	char filename[256];
	char input[256];

	unsigned char prevmode7[MODE7_MAX_SIZE];
	unsigned char delta[MODE7_MAX_SIZE];
	unsigned char mode7[MODE7_MAX_SIZE];

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

			int line[MODE7_WIDTH][8];
			int domcol[MODE7_WIDTH][2];

			for (int x7 = 0; x7 < MODE7_WIDTH; x7++)
			{
				for (int c = 0; c < 8; c++)
					line[x7][c] = 0;

				domcol[x7][0] = 0;
				domcol[x7][1] = 0;		// should it be 7 for white?
			}

			cimg_forX(src, x)
			{
				int x7 = (x / 2) + (MODE7_WIDTH - FRAME_WIDTH);

				line[x7][get_colour_from_rgb(src(x, y, 0), src(x, y, 1), src(x, y, 2))]++;
				line[x7][get_colour_from_rgb(src(x+1, y, 0), src(x+1, y, 1), src(x+1, y, 2))]++;
				line[x7][get_colour_from_rgb(src(x, y+1, 0), src(x, y+1, 1), src(x, y+1, 2))]++;
				line[x7][get_colour_from_rgb(src(x+1, y+1, 0), src(x+1, y+1, 1), src(x+1, y+1, 2))]++;
				line[x7][get_colour_from_rgb(src(x, y+2, 0), src(x, y+2, 1), src(x, y+2, 2))]++;
				line[x7][get_colour_from_rgb(src(x+1, y+2, 0), src(x+1, y+2, 1), src(x+1, y+2, 2))]++;

			//	printf("(%d, %d) = (0x%x, 0x%x, 0x%x)\n", x, y, src(x, y, 0), src(x, y, 1), src(x, y, 2));

				mode7[(y7 * MODE7_WIDTH) + (x7)] = 32															// bit 5 always set!
						+ (src(x, y, 0)				?  1 : 0)			// (x,y) = bit 0
						+ (src(x + 1, y, 0)			?  2 : 0)			// (x+1,y) = bit 1
						+ (src(x, y + 1, 0)			?  4 : 0)			// (x,y+1) = bit 2
						+ (src(x + 1, y + 1, 0)		?  8 : 0)			// (x+1,y+1) = bit 3
						+ (src(x, y + 2, 0)			? 16 : 0)			// (x,y+2) = bit 4
						+ (src(x + 1, y + 2, 0)		? 64 : 0);			// (x+1,y+2) = bit 6

				x++;


				int unique_colours = 0;
				int dominant_colour = 0;
				int max_count = 0;

				for (int c = 1; c < 8; c++)
				{
					if (line[x7][c]) unique_colours++;
					if (line[x7][c] && line[x7][c] >= max_count)			// must have a count to count!
					{
						max_count = line[x7][c];
						dominant_colour = c;
					}
				}

				if (line[x7][0])				// black always a background colour
				{
					domcol[x7][0] = 0;
					domcol[x7][1] = dominant_colour;
				}
				else
				{
					domcol[x7][0] = dominant_colour;

					max_count = 0;
					dominant_colour = 0;

					for (int c = 1; c < 8; c++)
					{
						if ( c!= domcol[x7][0] && line[x7][c] && line[x7][c] >= max_count)			// must have a count to count!
						{
							max_count = line[x7][c];
							dominant_colour = c;
						}
					}

					domcol[x7][1] = dominant_colour;
				}

#if _COLOUR_DEBUG
				printf("(%d, %d) = [%d %d %d %d %d %d %d %d] (u=%d bg=%d fg=%d)\n", x7, y7, line[x7][0], line[x7][1], line[x7][2], line[x7][3], line[x7][4], line[x7][5], line[x7][6], line[x7][7], unique_colours, domcol[x7][0], domcol[x7][1]);
#endif
			}

			int current_colour = 7;
			int current_fill = 0;

			for (int x7 = (MODE7_WIDTH - FRAME_WIDTH); x7 < MODE7_WIDTH; x7++)
			{
				if (domcol[x7][0] != current_fill)
				{
					if (domcol[x7][0] && domcol[x7][0] != current_colour)
					{
						current_colour = domcol[x7][0];
						mode7[y7*MODE7_WIDTH + x7 - 1] = 144 + current_colour;
#if _COLOUR_DEBUG
						printf("(%d, %d) Forced colour change %d\n", x7 - 1, y7, current_colour);
#endif
					}

					current_fill = domcol[x7][0];

					if (current_fill)
					{
						mode7[y7*MODE7_WIDTH + x7] = 157;				// new background
#if _COLOUR_DEBUG
						printf("(%d, %d) Forced fill\n", x7, y7);
#endif
					}
					else
					{
						mode7[y7*MODE7_WIDTH + x7] = 156;		// black background
#if _COLOUR_DEBUG
						printf("(%d, %d) Forced black background\n", x7, y7);
#endif
					}
				}

				// Need to switch colour
				if (domcol[x7][1])
				{
					if (domcol[x7][1] != current_colour)
					{
						if (mode7[y7*MODE7_WIDTH + x7 - 1] == MODE7_BLANK)
						{
							current_colour = domcol[x7][1];
							mode7[y7*MODE7_WIDTH + x7 - 1] = 144 + current_colour;
#if _COLOUR_DEBUG
							printf("(%d, %d) Inserting colour change %d\n", x7 - 1, y7, current_colour);
#endif
						}
						else
						{
#if _COLOUR_DEBUG
							printf("(%d, %d) Failed to insert change to colour %d\n", x7, y7, domcol[x7][1]);
#endif
						}
					}
				}
			}

			// printf("\n");

			y += 2;
		}

//		for (int i = 0; i < 1000; i++)
//		{
//			printf("0x%x ", mode7[i]);
//			if (i % 40 == 39)printf("\n");
//		}

		if (n == 1)
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

		if (numdeltas > FRAME_SIZE/3)
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
			numdeltabytes = numdeltas * 3;

			*ptr++ = LO(numdeltas);
			*ptr++ = HI(numdeltas);

			int previ = 0;

			for (int i = 0; i < FRAME_SIZE; i++)
			{
				if (delta[i] != 0)
				{
					unsigned char byte = mode7[i];			//  ^ prevmode7[i] for EOR with prev.
#if 0

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

	printf("\ntotal frames = %d\n", NUM_FRAMES);
	printf("frame size = %d\n", FRAME_SIZE);
	printf("total deltas = %d\n", totaldeltas);
	printf("total bytes = %d\n", totalbytes);
	printf("max deltas = %d\n", maxdeltas);
	printf("reset frames = %d\n", resetframes);
	printf("deltas / frame = %f\n", totaldeltas / (float)NUM_FRAMES);
	printf("bytes / frame = %f\n", totalbytes / (float)NUM_FRAMES);
	printf("bytes / second = %f\n", 25.0f * totalbytes / (float)NUM_FRAMES);
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

