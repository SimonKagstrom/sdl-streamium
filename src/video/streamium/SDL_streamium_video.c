/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_streamium_video.h"
#include "SDL_streamium_events_c.h"
#include "SDL_streamium_mouse_c.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* From the Linux driver */
#define IOCTRL_SEND_LCDCMD          0x100
#define IOCTRL_CLEAR_LCD            0x101
#define IOCTRL_DRAW_LINE            0x102
#define IOCTRL_SEND_STRING          0x103
#define IOCTRL_SETUPDATE_AREA       0x104
#define IOCTRL_UPDATE_NOW           0x105
#define IOCTRL_GET_LCD_WIDTH        0x106
#define IOCTRL_GET_LCD_HEIGHT       0x107
#define IOCTRL_GET_LCD_BPP          0x108
#define IOCTRL_RESET_LCD            0x109
#define IOCTRL_INC_LCD_CONTRAST     0x10A
#define IOCTRL_DEC_LCD_CONTRAST     0x10B
#define IOCTRL_SET_PID_SIGNAL       0x10C
#define IOCTRL_SET_LIGHTPIN_HIGH    0x10D
#define IOCTRL_SET_LIGHTPIN_LOW     0x10E
#define IOCTRL_GET_DISPLAY_REVERSE_FLAG  0x10F
#define IOCTRL_SET_LCD_CONTRAST     0x110
#define IOCTRL_SET_LCD_RELATIVE_CONTRAST 0x111
#define IOCTRL_GET_LCD_SEND_FINISH_FLAG  0x112
#define IOCTRL_SET_PPTR_FLAG        0x113
#define IOCTRL_DISPLAY_ENABLE       0x114
#define IOCTRL_DISPLAY_DISABLE      0x115
#define IOCTRL_GET_TFT_TYPE         0x116
//below is for debug test
#define IOCTRL_SEND_LCDDATABYTE     0x20F
#define IOCTRL_DISP_DATA            0x213
#define IOCTRL_WRITE_LCDCTRL_REG    0x214
#define IOCTRL_READ_LCDCTRL_REG     0x215
#define IOCTRL_WRITE_LCDCTRL_DATA   0x216


#define STREAMIUMVID_DRIVER_NAME "streamium"

/* Initialization/Query functions */
static int STREAMIUM_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **STREAMIUM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *STREAMIUM_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int STREAMIUM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void STREAMIUM_VideoQuit(_THIS);

/* Hardware surface functions */
static int STREAMIUM_FlipHWSurface(_THIS, SDL_Surface *surface);
static int STREAMIUM_AllocHWSurface(_THIS, SDL_Surface *surface);
static int STREAMIUM_LockHWSurface(_THIS, SDL_Surface *surface);
static void STREAMIUM_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void STREAMIUM_FreeHWSurface(_THIS, SDL_Surface *surface);

/* etc. */
static void STREAMIUM_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* DUMMY driver bootstrap functions */

static int STREAMIUM_Available(void)
{
	return(1);
}

static void STREAMIUM_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *STREAMIUM_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = STREAMIUM_VideoInit;
	device->ListModes = STREAMIUM_ListModes;
	device->SetVideoMode = STREAMIUM_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = STREAMIUM_SetColors;
	device->UpdateRects = STREAMIUM_UpdateRects;
	device->VideoQuit = STREAMIUM_VideoQuit;
	device->AllocHWSurface = STREAMIUM_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = STREAMIUM_LockHWSurface;
	device->UnlockHWSurface = STREAMIUM_UnlockHWSurface;
	device->FlipHWSurface = STREAMIUM_FlipHWSurface;
	device->FreeHWSurface = STREAMIUM_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = STREAMIUM_InitOSKeymap;
	device->PumpEvents = STREAMIUM_PumpEvents;

	device->free = STREAMIUM_DeleteDevice;

	return device;
}

VideoBootStrap STREAMIUM_bootstrap = {
	STREAMIUMVID_DRIVER_NAME, "SDL streamium video driver",
	STREAMIUM_Available, STREAMIUM_CreateDevice
};



int STREAMIUM_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	int screen_w, screen_h;
	int fd, bpp;

	fd = open("/dev/lcddev", O_RDWR);
	if (fd < 0)
		return -1;

	if (ioctl(fd, IOCTRL_GET_LCD_BPP, &bpp) < 0) {
		close(fd);
		return -1;
	}
	if (ioctl(fd, IOCTRL_GET_LCD_WIDTH, &screen_w) < 0) {
		close(fd);
		return -1;
	}
	if (ioctl(fd, IOCTRL_GET_LCD_HEIGHT, &screen_h) < 0) {
		close(fd);
		return -1;
	}

	ioctl(fd, IOCTRL_DISPLAY_ENABLE, NULL);
	ioctl(fd, IOCTRL_CLEAR_LCD, NULL);

	/* we change this during the SDL_SetVideoMode implementation... */
	vformat->BitsPerPixel = bpp;
	switch(bpp) {
	case 8:
		/* Untested! */
		vformat->BytesPerPixel = 1;
		break;
	case 16:
		/* Untested! */
		vformat->BytesPerPixel = 2;
		break;
	case 24:
		vformat->BytesPerPixel = 3;
		vformat->Rmask = 0xff;
		vformat->Rshift = 0;
		vformat->Gmask = 0xff00;
		vformat->Gshift = 8;
		vformat->Bmask = 0xff0000;
		vformat->Bshift = 16;
		break;
	case 32:
		/* Untested! */
		vformat->BytesPerPixel = 4;
		vformat->Rmask = 0xff0000;
		vformat->Rshift = 16;
		vformat->Gmask = 0xff00;
		vformat->Gshift = 8;
		vformat->Bmask = 0xff;
		vformat->Bshift = 0;
		break;
	default:
		close(fd);
		return -1;
	}

	this->hidden->fd = fd;
	this->hidden->screen_w = screen_w;
	this->hidden->screen_h = screen_h;
	this->hidden->bufsize = screen_w * screen_h * vformat->BytesPerPixel;

	/* We're done! */
	return(0);
}

SDL_Rect **STREAMIUM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	static SDL_Rect r;
	static SDL_Rect *rs[2] = { &r, 0 };

	r.x = 0;
	r.y = 0;
	r.w = this->hidden->screen_w;
	r.h = this->hidden->screen_h;

	return (SDL_Rect **)rs;
}

SDL_Surface *STREAMIUM_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	fprintf(stderr, "XXX: Set video\n");
	if (height != this->hidden->screen_h || width != this->hidden->screen_w) {
		SDL_SetError("Video mode without height");
		return NULL;
	}

	if ( this->hidden->buffer )
		munmap( this->hidden->buffer, this->hidden->bufsize );

	this->hidden->buffer = mmap(NULL, this->hidden->bufsize, PROT_WRITE, MAP_SHARED,
			this->hidden->fd, 0);
	if ( ! this->hidden->buffer ) {
		SDL_SetError("Couldn't map buffer for requested mode");
		return(NULL);
	}

/* 	printf("Setting mode %dx%d\n", width, height); */
	SDL_memset(this->hidden->buffer, 0, this->hidden->bufsize);

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, 0xff, 0xff00, 0xff0000, 0) ) {
		munmap(this->hidden->buffer, this->hidden->bufsize);
		this->hidden->buffer = NULL;
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

	/* Set up the new mode framebuffer */
	current->flags = (flags & SDL_FULLSCREEN) | SDL_HWSURFACE | SDL_DOUBLEBUF;
	this->hidden->w = current->w = width;
	this->hidden->h = current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->pixels = this->hidden->buffer;

	/* We're done */
	return(current);
}

static int STREAMIUM_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	ioctl(this->hidden->fd, IOCTRL_UPDATE_NOW, NULL);
	return 0;
}


static int STREAMIUM_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return 0;
}

static void STREAMIUM_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* We need to wait for vertical retrace on page flipped displays */
static int STREAMIUM_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void STREAMIUM_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static void STREAMIUM_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
}

int STREAMIUM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* do nothing of note. */
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void STREAMIUM_VideoQuit(_THIS)
{
	if (this->hidden->buffer)
		munmap(this->hidden->buffer, this->hidden->bufsize);

	close(this->hidden->fd);
}
