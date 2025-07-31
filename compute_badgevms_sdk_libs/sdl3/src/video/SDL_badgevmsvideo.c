/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2025 HP van Braam <hp@tmm.cx>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_BADGEVMS

#include "../../SDL3/src/events/SDL_events_c.h"
#include "../../SDL3/src/video/SDL_pixels_c.h"
#include "../../SDL3/src/video/SDL_sysvideo.h"

#include "SDL_badgevmsevents_c.h"
#include "SDL_badgevmsframebuffer_c.h"
#include "SDL_badgevmsvideo.h"

#define BADGEVMSVID_DRIVER_NAME "badgevms"

static bool BADGEVMS_VideoInit(SDL_VideoDevice *_this);
static void BADGEVMS_VideoQuit(SDL_VideoDevice *_this);

bool BADGEVMS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *data;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!data) {
        return false;
    }

    window_size_t size = { window->w, window->h };
    data->badgevms_window = window_create(window->title, size, WINDOW_FLAG_NONE);
    if (!data->badgevms_window) {
        SDL_free(data);
        return SDL_SetError("Could not create BadgeVMS window");
    }

    data->window = window;
    window->internal = data;

    return true;
}

static void BADGEVMS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data) {
        if (data->badgevms_window) {
            window_destroy(data->badgevms_window);
        }
        SDL_free(data);
        window->internal = NULL;
    }
}

static bool BADGEVMS_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, window->pending.x, window->pending.y);
    return true;
}

static void BADGEVMS_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window->pending.w, window->pending.h);
}

static bool BADGEVMS_Available(const char *enable_hint)
{
    return true;
}

static void BADGEVMS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *BADGEVMS_InternalCreateDevice(const char *enable_hint)
{
    SDL_VideoDevice *device;

    if (!BADGEVMS_Available(enable_hint)) {
        return NULL;
    }

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    device->is_dummy = false;

    device->VideoInit = BADGEVMS_VideoInit;
    device->VideoQuit = BADGEVMS_VideoQuit;
    device->PumpEvents = BADGEVMS_PumpEvents;
    device->SetWindowSize = BADGEVMS_SetWindowSize;
    device->SetWindowPosition = BADGEVMS_SetWindowPosition;
    device->CreateSDLWindow = BADGEVMS_CreateWindow;
    device->DestroyWindow = BADGEVMS_DestroyWindow;
    device->CreateWindowFramebuffer = SDL_BADGEVMS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_BADGEVMS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_BADGEVMS_DestroyWindowFramebuffer;

    device->GetDisplayModes = NULL; // Use default
    device->SetDisplayMode = NULL;  // Use default
    device->free = BADGEVMS_DeleteDevice;

    printf("BADGEVMS: Video device created successfully\n");
    return device;
}

static SDL_VideoDevice *BADGEVMS_CreateDevice(void)
{
    return BADGEVMS_InternalCreateDevice(BADGEVMSVID_DRIVER_NAME);
}

VideoBootStrap BADGEVMS_bootstrap = {
    BADGEVMSVID_DRIVER_NAME, "SDL BadgeVMS video driver",
    BADGEVMS_CreateDevice,
    NULL, // no ShowMessageBox implementation
    false
};

bool BADGEVMS_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    SDL_DisplayID display_id;

    printf("BADGEVMS_VideoInit: Starting initialization...\n");

    SDL_zero(mode);
    pixel_format_t tmp;
    // BadgeVMS pixel formats are the same as SDL3
    get_screen_info(&mode.w, &mode.h, ((pixel_format_t *)&mode.format), &mode.refresh_rate);

    printf("BADGEVMS_VideoInit: Adding display mode %dx%d, format=%s\n",
           mode.w, mode.h, SDL_GetPixelFormatName(mode.format));

    display_id = SDL_AddBasicVideoDisplay(&mode);
    if (display_id == 0) {
        printf("BADGEVMS_VideoInit: SDL_AddBasicVideoDisplay FAILED! Error: %s\n", SDL_GetError());
        return false;
    }

    printf("BADGEVMS_VideoInit: Successfully added display ID %u\n", (unsigned int)display_id);
    return true;
}

void BADGEVMS_VideoQuit(SDL_VideoDevice *_this)
{
}

#endif // SDL_VIDEO_DRIVER_BADGEVMS
