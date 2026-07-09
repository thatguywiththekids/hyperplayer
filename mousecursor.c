#include "mousecursor.h"
#include "portability.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>

static SDL_Cursor *g_cursor = NULL;

bool mousecursor_load(const wchar_t *pngPath, int hotspotX, int hotspotY)
{
    char path[MAX_PATH*4];
    wcstombs(path, pngPath, sizeof(path));
    
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) return false;
    
    if (g_cursor) SDL_DestroyCursor(g_cursor);
    g_cursor = SDL_CreateColorCursor(surface, hotspotX, hotspotY);
    SDL_DestroySurface(surface);
    
    return g_cursor != NULL;
}

void mousecursor_unload(void)
{
    if (g_cursor) {
        SDL_DestroyCursor(g_cursor);
        g_cursor = NULL;
    }
}

void mousecursor_apply(void)
{
    if (g_cursor) {
        SDL_SetCursor(g_cursor);
    }
}
