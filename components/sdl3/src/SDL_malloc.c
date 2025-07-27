/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

extern void *why_malloc(size_t size);
extern void why_free(void *ptr);
extern void *why_calloc(size_t nmemb, size_t size);
extern void *why_realloc(void *ptr, size_t size);
extern void *why_reallocarray(void * ptr, size_t nmemb, size_t size);

/* This file contains portable memory management functions for SDL */

#define real_malloc  why_malloc
#define real_calloc  why_calloc
#define real_realloc why_realloc
#define real_free    why_free

// mark the allocator entry points as KEEPALIVE so we can call these from JavaScript.
// otherwise they could could get so aggressively inlined that their symbols
// don't exist at all in the final binary!
#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
extern SDL_DECLSPEC SDL_MALLOC EMSCRIPTEN_KEEPALIVE void *SDLCALL SDL_malloc(size_t size);
extern SDL_DECLSPEC SDL_MALLOC SDL_ALLOC_SIZE2(1, 2) EMSCRIPTEN_KEEPALIVE void *SDLCALL SDL_calloc(size_t nmemb, size_t size);
extern SDL_DECLSPEC SDL_ALLOC_SIZE(2) EMSCRIPTEN_KEEPALIVE void *SDLCALL SDL_realloc(void *mem, size_t size);
extern SDL_DECLSPEC EMSCRIPTEN_KEEPALIVE void SDLCALL SDL_free(void *mem);
#endif

/* Memory functions used by SDL that can be replaced by the application */
static struct
{
    SDL_malloc_func malloc_func;
    SDL_calloc_func calloc_func;
    SDL_realloc_func realloc_func;
    SDL_free_func free_func;
    SDL_AtomicInt num_allocations;
} s_mem = {
    real_malloc, real_calloc, real_realloc, real_free, { 0 }
};

// Define this if you want to track the number of allocations active
// #define SDL_TRACK_ALLOCATION_COUNT
#ifdef SDL_TRACK_ALLOCATION_COUNT
#define INCREMENT_ALLOCATION_COUNT() (void)SDL_AtomicIncRef(&s_mem.num_allocations)
#define DECREMENT_ALLOCATION_COUNT() (void)SDL_AtomicDecRef(&s_mem.num_allocations)
#else
#define INCREMENT_ALLOCATION_COUNT()
#define DECREMENT_ALLOCATION_COUNT()
#endif

void SDL_GetOriginalMemoryFunctions(SDL_malloc_func *malloc_func,
                                    SDL_calloc_func *calloc_func,
                                    SDL_realloc_func *realloc_func,
                                    SDL_free_func *free_func)
{
    if (malloc_func) {
        *malloc_func = real_malloc;
    }
    if (calloc_func) {
        *calloc_func = real_calloc;
    }
    if (realloc_func) {
        *realloc_func = real_realloc;
    }
    if (free_func) {
        *free_func = real_free;
    }
}

void SDL_GetMemoryFunctions(SDL_malloc_func *malloc_func,
                            SDL_calloc_func *calloc_func,
                            SDL_realloc_func *realloc_func,
                            SDL_free_func *free_func)
{
    if (malloc_func) {
        *malloc_func = s_mem.malloc_func;
    }
    if (calloc_func) {
        *calloc_func = s_mem.calloc_func;
    }
    if (realloc_func) {
        *realloc_func = s_mem.realloc_func;
    }
    if (free_func) {
        *free_func = s_mem.free_func;
    }
}

bool SDL_SetMemoryFunctions(SDL_malloc_func malloc_func,
                            SDL_calloc_func calloc_func,
                            SDL_realloc_func realloc_func,
                            SDL_free_func free_func)
{
    if (!malloc_func) {
        return SDL_InvalidParamError("malloc_func");
    }
    if (!calloc_func) {
        return SDL_InvalidParamError("calloc_func");
    }
    if (!realloc_func) {
        return SDL_InvalidParamError("realloc_func");
    }
    if (!free_func) {
        return SDL_InvalidParamError("free_func");
    }

    s_mem.malloc_func = malloc_func;
    s_mem.calloc_func = calloc_func;
    s_mem.realloc_func = realloc_func;
    s_mem.free_func = free_func;
    return true;
}

int SDL_GetNumAllocations(void)
{
#ifdef SDL_TRACK_ALLOCATION_COUNT
    return SDL_GetAtomicInt(&s_mem.num_allocations);
#else
    return -1;
#endif
}

void *SDL_malloc(size_t size)
{
    void *mem;

    if (!size) {
        size = 1;
    }

    mem = s_mem.malloc_func(size);
    if (mem) {
        INCREMENT_ALLOCATION_COUNT();
    } else {
        SDL_OutOfMemory();
    }

    return mem;
}

void *SDL_calloc(size_t nmemb, size_t size)
{
    void *mem;

    if (!nmemb || !size) {
        nmemb = 1;
        size = 1;
    }

    mem = s_mem.calloc_func(nmemb, size);
    if (mem) {
        INCREMENT_ALLOCATION_COUNT();
    } else {
        SDL_OutOfMemory();
    }

    return mem;
}

void *SDL_realloc(void *ptr, size_t size)
{
    void *mem;

    if (!size) {
        size = 1;
    }

    mem = s_mem.realloc_func(ptr, size);
    if (mem && !ptr) {
        INCREMENT_ALLOCATION_COUNT();
    } else if (!mem) {
        SDL_OutOfMemory();
    }

    return mem;
}

void SDL_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    s_mem.free_func(ptr);
    DECREMENT_ALLOCATION_COUNT();
}
