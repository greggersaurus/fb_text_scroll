/**
 * \file main.cpp
 * \author Greg Gluszek
 * \brief Example program for using a the Raspberry PI Sense HAT to 
 *  output scrolling text. Portions of this code are based on the Sense HAT
 *  snake example code (credit is given below where due). 
 */

#include <linux/fb.h>
#include <linux/input.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <poll.h>
#include <dirent.h>

//!< Defines necessary attributes to draw glyp to screen.
struct tsBdfChar
{
	const struct 
	{
		int x;
		int y;
	} msDWidth; //!< Device Width of the glyph.
	const struct
	{
		int x;
		int y;
		int x_offset;
		int y_offset;
	} msBBX; //!< Bounding box of the glyph.
	const uint8_t* const maBitmap; //!< Bitmap of the glyph.
};

#include "bitmap_array.h"
#include "bdfchars.h"
#include "bdfchars_lut.h"

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"
#define DEV_FB "/dev"
#define FB_DEV_NAME "fb"

// Member variables
void* mpFbMem = NULL; //!< Memory mapped access to frame buffer memory.
struct fb_fix_screeninfo msFbFixInfo; //!< Fixed info for open frame buffer.
struct fb_var_screeninfo msFbVarInfo; //!< Variable info for open frame buffer.

struct pollfd msPollFd; //!< Used for polling input events

uint32_t mnPixelVal = 0x0018; //!< Color of characters being drawn to screen.
uint32_t mnMsecSleep = 100; //!< Sleep time in milliseconds between column 
	// outputs.

bool* mpColData = NULL; //!< Buffer where column data to be drawn to screen is 
	// stored.

/**
 * Taken from /usr/src/sense-hat/examples/snake/snake.c
 */
static int is_event_device(const struct dirent *dir)
{
	return strncmp(EVENT_DEV_NAME, dir->d_name,
		       strlen(EVENT_DEV_NAME)-1) == 0;
}
/**
 * Taken from /usr/src/sense-hat/examples/snake/snake.c
 */
static int is_framebuffer_device(const struct dirent *dir)
{
	return strncmp(FB_DEV_NAME, dir->d_name,
		       strlen(FB_DEV_NAME)-1) == 0;
}

/**
 * Function to find and open an event device with a particular ID.
 * Taken from /usr/src/sense-hat/examples/snake/snake.c
 */
static int open_evdev(const char *dev_name)
{
	struct dirent **namelist;
	int i, ndev;
	int fd = -1;

	ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);
	if (ndev <= 0)
		return ndev;

	for (i = 0; i < ndev; i++)
	{
		char fname[64];
		char name[256];

		snprintf(fname, sizeof(fname),
			 "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd < 0)
			continue;
		ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		if (strcmp(dev_name, name) == 0)
			break;
		close(fd);
	}

	for (i = 0; i < ndev; i++)
		free(namelist[i]);

	return fd;
}

/**
 * Function to find and open a framebuffer device with a particular ID.
 * Taken from /usr/src/sense-hat/examples/snake/snake.c
 */
static int open_fbdev(const char *dev_name)
{
	struct dirent **namelist;
	int i, ndev;
	int fd = -1;
	struct fb_fix_screeninfo fix_info;

	ndev = scandir(DEV_FB, &namelist, is_framebuffer_device, versionsort);
	if (ndev <= 0)
		return ndev;

	for (i = 0; i < ndev; i++)
	{
		char fname[64];

		snprintf(fname, sizeof(fname),
			 "%s/%s", DEV_FB, namelist[i]->d_name);
		fd = open(fname, O_RDWR);
		if (fd < 0)
			continue;
		ioctl(fd, FBIOGET_FSCREENINFO, &fix_info);
		if (strcmp(dev_name, fix_info.id) == 0)
			break;
		close(fd);
		fd = -1;
	}
	for (i = 0; i < ndev; i++)
		free(namelist[i]);

	return fd;
}

/**
 * Function to handle joystick events. Change text color on up and down, change
 *  scroll speedon left and right.
 * Taken (mostly) from /usr/src/sense-hat/examples/snake/snake.c
 */
void handle_events(int evfd)
{
	struct input_event ev[64];
	int i, rd;

	rd = read(evfd, ev, sizeof(struct input_event) * 64);
	if (rd < (int) sizeof(struct input_event)) 
	{
		fprintf(stderr, "expected %d bytes, got %d\n",
		        (int) sizeof(struct input_event), rd);
		return;
	}
	for (i = 0; i < rd / (int)sizeof(struct input_event); i++) 
	{
		if (ev->type != EV_KEY)
			continue;
		if (ev->value != 1)
			continue;
		switch (ev->code) 
		{
		case KEY_UP:
			// Please excuse this hokeyness.
			// What's going on here is I am assuming RGB 565 and 
			//  cycling through 32 combinations with as many color 
			//  variations as possible by effectively only using the
			//  upper bits of each color channel.
			// This is also coupled with each event code being 
			//  called twice per joystick move for some reason. This
			//  works, but could be clearer and handle more 
			//  scenarios of bits per pixel
			if (mnPixelVal < 0xC61C)
			{
				mnPixelVal |= 0x39E3;
				mnPixelVal += 1;
				mnPixelVal &= 0xC61C;
			}
			break;

		case KEY_RIGHT:
			if (mnMsecSleep >= 5)
				mnMsecSleep -= 5;
			break;

		case KEY_DOWN:
			// See comments for KEY_UP event
			if (mnPixelVal > 0x0008)
				mnPixelVal -= 1;
			mnPixelVal &= 0xC61C;
			break;

		case KEY_LEFT:
			mnMsecSleep += 5;
			break;
		}
	}
}

/**
 * Draw column on righttmost portion of screen and advance framebuffer columns 
 *  to the left.
 *
 * \param[in] apColData Array of data to draw in column. True to draw on that 
 *   pixel. False to leave empty.
 *
 * \return None.
 */
void drawCol(bool* apColData)
{
	int bytes_per_pixel = msFbVarInfo.bits_per_pixel/8;
	// Pointer to iterate over rightmost column of pixel data
	void* fb_right_col_mem = (uint8_t*)mpFbMem + bytes_per_pixel
		* (msFbVarInfo.xres-1);
	uint64_t pixel_data = 0;

	// First check for inputs that may change draw speed or pixel color
	while (poll(&msPollFd, 1, 0) > 0)
		handle_events(msPollFd.fd);

	// Traverse height of framebuffer
	for (uint32_t y = 0; y < msFbVarInfo.yres; y++) 
	{
		// Walk all columns over one to the left, leaving the 
		//  rightmost column untouched
		memcpy((uint8_t*)mpFbMem + y*msFbFixInfo.line_length, 
			(uint8_t*)mpFbMem + y*msFbFixInfo.line_length 
			+ bytes_per_pixel, msFbFixInfo.line_length);

		// Fill the rightmost column based on given column data
		if (apColData[y])
		{
			pixel_data = mnPixelVal;
		}
		else
		{
			pixel_data = 0;
		}
		memcpy(fb_right_col_mem, &pixel_data, bytes_per_pixel);
		fb_right_col_mem = (uint8_t*)fb_right_col_mem + bytes_per_pixel
			* msFbVarInfo.xres;
	}

	// Sleep after outputting new column
	usleep(mnMsecSleep * 1000);
}

/**
 * Output the given character to the frame buffer column by column so that text 
 *  scrolls right to left.
 *
 * \param[in] The character to output to screen.
 *
 * \return None.
 */
void outputChar(const tsBdfChar* apChar)
{
	uint8_t mask = 0x80; //!< Mask to read out columns of character from 
		// bitmap array
	uint32_t fb_draw_start_row = 0; //!< At what row offset from top
		//!< to start drawing in frame buffer.
	uint32_t bitmap_array_width = 0; //!< Width of bitmap array so we 
		//!< know where one row ends and the next begins
	uint32_t char_width_byte_cntr = 0; //!< Counts which byte of the
		//!< bitmap row we are outputting
	uint32_t char_draw_width = 0; //!< Number of columns for character, 
		//!< including space to next character

	// Check that valid character was given
	//  TODO: print default character in case of NULL?
	if (!apChar)
		return;

	if (apChar->msBBX.y < (int)msFbVarInfo.yres)
	{
		// If screen is taller than font, start drawing character font 
		//  at bottom of screen 
		fb_draw_start_row = msFbVarInfo.yres - apChar->msBBX.y; 
	}

	// Calculate bytes per row in bitmap array
	bitmap_array_width = apChar->msBBX.x / 8;
	bitmap_array_width += (apChar->msBBX.x % 8)?1:0;

	// Loop for the number of columns to output for each character
	char_draw_width = apChar->msDWidth.x; 
	if (apChar->msDWidth.x <= apChar->msBBX.x)
	{
		char_draw_width += apChar->msBBX.x - apChar->msDWidth.x;
	}

	// Output blank columns for taking into account bounding box x_offset
	memset(mpColData, 0, msFbVarInfo.yres*sizeof(mpColData[0]));
	for (int col_cntr = 0; col_cntr < apChar->msBBX.x_offset; col_cntr++)
	{
		drawCol(mpColData);
	}

	// Loop for the number of columns to output for each character.
	for (int char_width_col_cntr = apChar->msBBX.x_offset; 
		char_width_col_cntr < (int)char_draw_width; 
		char_width_col_cntr++)
	{
		// Handle rows above character draw start
		for (uint32_t y = 0; y < fb_draw_start_row; y++) 
		{
			mpColData[y] = false;
		}
		// Handle rows with potentially new character data in them
		for (uint32_t y = fb_draw_start_row; y < msFbVarInfo.yres; y++) 
		{
			mpColData[y] = apChar->maBitmap[(y-fb_draw_start_row) *
				bitmap_array_width + char_width_byte_cntr] & 
				mask;
		}

		// Draw the column we just defined
		drawCol(mpColData);

		// There is still more data to output for this character
		// Check if there is another byte in the bitmap array
		if (0x01 == mask)
		{
			char_width_byte_cntr++;
			if (char_width_byte_cntr < bitmap_array_width)
			{
				// If there are more bytes on this row, reset 
				//  the mask to iterate over those columns
				mask = 0x80;
			}
		}

		// Shift the mask to output the next column of the character
		mask >>= 1;
	}
}

/**
 * Application entry point.
 *
 * \param argc Number of command line arguments.
 * \param argv Array on input arguments. argv[1] optionally specifies a text
 * 	file to read messages from that will be scrolled out on the frame
 *	buffer.
 *
 * TODO: use EXIT_FAILURE, etc. macros.
 * \return 0 on success. 1 on failure.
 */
int main(int argc, char* argv[]) 
{
	// File descriptor for frame buffer device
	int fb_fd = -1;
	// Frame buffer size in bytes
	int fb_mem_size = 0;
	const uint32_t PRINT_STR_LEN = 256;
	// String to print to frame buffer
	char print_str[PRINT_STR_LEN] = {0};
	FILE *message_file = NULL;

	// Open the framebuffer device in read write mode
	fb_fd = open_fbdev("RPi-Sense FB");
	if (fb_fd < 0) 
	{
		printf("Unable to find/open frame buffer.\n");
		return 1;
	}

	// Ioctl to retrieve fixed screen info. 
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &msFbFixInfo) < 0) 
	{
		printf("get fixed screen info failed: %s\n",
			strerror(errno));
		close(fb_fd);
		return 1;
	}

	// Ioctl to get the variable screen info. 
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &msFbVarInfo) < 0) 
	{
		printf("Unable to retrieve variable screen info: %s\n",
			  strerror(errno));
		close(fb_fd);
		return 1;
	}

	// Calculate the size to mmap 
	fb_mem_size = msFbFixInfo.line_length * msFbVarInfo.yres;
	printf("Line length %d\n", msFbFixInfo.line_length);
	printf("xres =  %d\n", msFbVarInfo.xres);
	printf("yres =  %d\n", msFbVarInfo.yres);
	printf("bits_per_pixel =  %d\n", msFbVarInfo.bits_per_pixel);

	// Now mmap the framebuffer. 
	mpFbMem = mmap(NULL, fb_mem_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fb_fd, 0);
	if (!mpFbMem) 
	{
		printf("mmap failed:\n");
		close(fb_fd);
		return 1;
	}

	// Clear screen
	memset(mpFbMem, 0, fb_mem_size);

	// Create buffer for column data to be drawn to screen
	mpColData = new bool[msFbVarInfo.yres];
	if (!mpColData)
	{
		printf("Not able to allocate memory for column data.\n");
		close(fb_fd);
		return 1;
	}

	msPollFd.events = POLLIN;

	msPollFd.fd = open_evdev("Raspberry Pi Sense HAT Joystick");
	if (msPollFd.fd < 0) 
	{
		fprintf(stderr, "Event device not found.\n");
		return msPollFd.fd;
	}

	if (argc < 2)
	{
		snprintf(print_str, PRINT_STR_LEN, "No input file provided! "
			"#toobad ");
	}
	else
	{
		// Open file to read text from
		message_file = fopen(argv[1], "r");
		if (!message_file)
		{
			snprintf(print_str, PRINT_STR_LEN, "Could not open file"
				" \"%s\"! #awnuts ", argv[1]);
		}
		else
		{
			// Get a line from the file
			fgets(print_str, PRINT_STR_LEN-1, message_file);
			print_str[PRINT_STR_LEN-1] = 0;
		}
	}

	// Loop through and print 
	// TODO: add exit condition or control?
	while (1)
	{
		// Print text buffered in print_str
		for (uint32_t ii = 0; ii < PRINT_STR_LEN; ii++)
		{
			// Stop printing buffer on NULL
			if (!print_str[ii])
				break;
			outputChar(BDF_CHARS_LUT[(uint8_t)print_str[ii]]);
		}

		// Check if we need to buffer more text 
		if (message_file)
		{
			// Start back at beginning of file if necessary
			if (feof(message_file))
				rewind(message_file);
			// Get a line from the file
			fgets(print_str, PRINT_STR_LEN-1, message_file);
			// Null termination safety 
			//TODO: strl function to do this automatically??
			print_str[PRINT_STR_LEN-1] = 0;
		}
	}

	//TODO: It is impossible to reach this code. See infinite loop above.
	return 0;
}

