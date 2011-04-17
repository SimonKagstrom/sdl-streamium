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

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_streamium_video.h"
#include "SDL_streamium_events_c.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * Function command codes for ioctl, from the kernel sources.
 */
#define BUTTONS_GET_WAVE		1
#define BUTTONS_SET_WAVE		2
#define BUTTONS_GET_KEY	        3
#define BUTTONS_SET_KEY	        4

#define BUTTONS_SET_SIGNAL      5
#define BUTTONS_GET_FLAGS		6
#define BUTTONS_SET_KEYPAD_TYPE 7
#define BUTTONS_SET_RAW_KEY_MAPTABLE 8
#define BUTTONS_GET_RAW_KEY_MAPTABLE 9
#define BUTTONS_GET_KEY_MAPTBL_SIZE 10
#define BUTTONS_SET_DEBUG_FLAG	11

#define BUTTONS_FLAGS_WAVE		0x01
#define BUTTONS_FLAGS_KEY		0x02

enum key_t_{
	Virtual_Key_Idle,

	/* 0x1 */
	physical_KEY_DBB,
	physical_KEY_IS,
	physical_KEY_MUTE,
	physical_key_Left_Reverse,
	physical_KEY_PLAY,
	physical_key_Right_Forward,
	physical_KEY_STOP,

	/* 0x8 */
	physical_KEY_SOURCE,
	physical_key_Down_Next,
	physical_key_Mark,
	physical_KEY_VOLUME_UP,
	physical_KEY_VOLUME_DOWN,
	physical_key_Music_broadcast,
	physical_key_Music_follows,
	physical_key_REC,


	/* 0x10 */
	physical_KEY_POWER_STANDBY,
	physical_key_Like_Artist,
	physical_key_Like_Genre,
	physical_key_Match_Genre,
	physical_key_Menu,
	physical_key_Up_Previous,
	physical_key_View,
	physical_key_Dim,

	/* 0x18 */
	virtual_VIEW_UP,
	virtual_VIEW_DOWN,
	virtual_PLAY_UP,
	virtual_PLAY_DOWN,
	virtual_VIEW_SOURCE,	// add this def under Vincent's demand
	virtual_DBB_SOURCE,

	/* 0x20 */
	unknown_key,
	unknown_key2,
	physical_key_RIGHT,
	physical_key_LEFT,
	physical_key_EJECT,
	physical_key_ERROR_ADVALUE,	//add by alex

	MAX_KEYS,
};


void STREAMIUM_PumpEvents(_THIS)
{
	unsigned int data;
	int last_pressed;
	key_t key;
	int i;

	/* do nothing. */
	if (ioctl(this->hidden->buttons_fd, BUTTONS_GET_KEY, &data) < 0) {
		return;
	}
	key = (key_t)(data & 0xff);

	for (i = 0; i < MAX_KEYS; i++) {
		SDL_keysym keysym;

		keysym.mod = KMOD_NONE;
		keysym.sym = this->hidden->keymap[i];
		keysym.scancode = SDLK_UNKNOWN;
		keysym.unicode = 0;

		/* Pressed or released */
		if (i == key && !this->hidden->current_keys[i]) {
			SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
			this->hidden->current_keys[key] = 1; /* Pressed */
		} else if (i != key && this->hidden->current_keys[i]) {
			SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
			this->hidden->current_keys[i] = 0;
		}
	}
}

void STREAMIUM_InitOSKeymap(_THIS)
{
	SDLKey *keymap = this->hidden->keymap;
	int i;

	this->hidden->buttons_fd = open("/dev/buttons", O_RDWR);
	if (this->hidden->buttons_fd < 0) {
			SDL_SetError("Couldn't open buttons file");
			return;
	}

	memset(this->hidden->current_keys, 0, sizeof(this->hidden->current_keys));
	for (i = 0; i < 256; i++)
		keymap[i] = SDLK_UNKNOWN;

	keymap[Virtual_Key_Idle] = SDLK_UNKNOWN;
	keymap[physical_KEY_DBB] = SDLK_UNKNOWN;
	keymap[physical_KEY_IS] = SDLK_UNKNOWN;
	keymap[physical_KEY_MUTE] = SDLK_UNKNOWN;
	keymap[physical_key_Left_Reverse] = SDLK_LEFT;
	keymap[physical_KEY_PLAY] = SDLK_RETURN;
	keymap[physical_key_Right_Forward] = SDLK_RIGHT;
	keymap[physical_KEY_STOP] = SDLK_ESCAPE;

	keymap[physical_KEY_SOURCE] = SDLK_HOME;
	keymap[physical_key_Down_Next] = SDLK_DOWN;
	keymap[physical_key_Mark] = SDLK_UNKNOWN;
	keymap[physical_KEY_VOLUME_UP] = SDLK_PLUS;
	keymap[physical_KEY_VOLUME_DOWN] = SDLK_MINUS;
	keymap[physical_key_Music_broadcast] = SDLK_UNKNOWN;
	keymap[physical_key_Music_follows] = SDLK_UNKNOWN;
	keymap[physical_key_REC] = SDLK_UNKNOWN;


	keymap[physical_KEY_POWER_STANDBY] = SDLK_UNKNOWN;
	keymap[physical_key_Like_Artist] = SDLK_UNKNOWN;
	keymap[physical_key_Like_Genre] = SDLK_UNKNOWN;
	keymap[physical_key_Match_Genre] = SDLK_UNKNOWN;
	keymap[physical_key_Menu] = 'M';
	keymap[physical_key_Up_Previous] = SDLK_UP;
	keymap[physical_key_View] = SDLK_UNKNOWN;
	keymap[physical_key_Dim] = SDLK_UNKNOWN;

	keymap[virtual_VIEW_UP] = SDLK_UP;
	keymap[virtual_VIEW_DOWN] = SDLK_DOWN;
	keymap[virtual_PLAY_UP] = SDLK_UNKNOWN;
	keymap[virtual_PLAY_DOWN] = SDLK_UNKNOWN;
	keymap[virtual_VIEW_SOURCE] = SDLK_UNKNOWN;
	keymap[virtual_DBB_SOURCE] = SDLK_UNKNOWN;

	keymap[physical_key_RIGHT] = SDLK_PAGEDOWN;
	keymap[physical_key_LEFT] = SDLK_PAGEUP;
	keymap[physical_key_EJECT] = SDLK_UNKNOWN;
	keymap[physical_key_ERROR_ADVALUE] = SDLK_UNKNOWN;
}

