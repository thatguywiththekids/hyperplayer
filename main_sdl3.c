#include "app.h"
#include "hp_app.h"
#include "renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static bool g_running = true;

static HP_KeyCode map_sdl_key(SDL_Keycode sym)
{
    switch (sym) {
        case SDLK_ESCAPE: return HP_KEY_ESCAPE;
        case SDLK_SPACE:  return HP_KEY_SPACE;
        case SDLK_LEFT:   return HP_KEY_LEFT;
        case SDLK_RIGHT:  return HP_KEY_RIGHT;
        case SDLK_UP:     return HP_KEY_UP;
        case SDLK_DOWN:   return HP_KEY_DOWN;
        case SDLK_S:      return HP_KEY_S;
        case SDLK_R:      return HP_KEY_R;
        default:          return HP_KEY_UNKNOWN;
    }
}

static uint32_t map_sdl_modifiers(SDL_Keymod mod)
{
    uint32_t modifiers = HP_KEYMOD_NONE;
    if (mod & SDL_KMOD_CTRL)  modifiers |= HP_KEYMOD_CTRL;
    if (mod & SDL_KMOD_ALT)   modifiers |= HP_KEYMOD_ALT;
    if (mod & SDL_KMOD_SHIFT) modifiers |= HP_KEYMOD_SHIFT;
    return modifiers;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    AppState app = {0};
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    g_window = SDL_CreateWindow("Hyperplayer (SDL3)", 1920, 1080, SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderLogicalPresentation(g_renderer, 1920, 1080, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    app.hwnd = (HWND)g_window;

    HP_DrawContext ctx;
    hp_renderer_init_context(&ctx, g_renderer);

    if (!hp_app_init(&app, &ctx)) {
        fprintf(stderr, "hp_app_init failed\n");
        return 1;
    }

    SDL_Event event;
    uint64_t lastTime = SDL_GetTicksNS();
    const int targetFps = 60;
    const int frameDelay = 1000 / targetFps;
    
    while (g_running) {
        uint64_t frameStart = SDL_GetTicks();
        uint64_t currentTime = SDL_GetTicksNS();
        double dt = (double)(currentTime - lastTime) / 1000000000.0;
        lastTime = currentTime;

        if (dt > 0.1) dt = 0.1; // Cap dt

        while (SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(g_renderer, &event);
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    g_running = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    hp_app_on_mouse_motion(&app, (int)event.motion.x, (int)event.motion.y);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    {
                        HP_MouseButton btn = HP_MOUSE_BUTTON_LEFT;
                        if (event.button.button == SDL_BUTTON_LEFT) btn = HP_MOUSE_BUTTON_LEFT;
                        else if (event.button.button == SDL_BUTTON_MIDDLE) btn = HP_MOUSE_BUTTON_MIDDLE;
                        else if (event.button.button == SDL_BUTTON_RIGHT) btn = HP_MOUSE_BUTTON_RIGHT;
                        hp_app_on_mouse_down(&app, btn, (int)event.button.x, (int)event.button.y);
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    hp_app_on_mouse_wheel(&app, event.wheel.x, event.wheel.y);
                    break;
                case SDL_EVENT_KEY_DOWN:
                    {
                        bool quit = false;
                        hp_app_on_key_down(
                            &app, 
                            map_sdl_key(event.key.key), 
                            map_sdl_modifiers(event.key.mod), 
                            &quit
                        );
                        if (quit) g_running = false;
                    }
                    break;
            }
        }

        hp_app_update(&app, dt);

        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);

        HP_Rect clientRect = { 0, 0, 1920, 1080 };
        hp_app_draw(&app, &ctx, &clientRect);

        SDL_RenderPresent(g_renderer);

        uint64_t frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > frameTime) {
            SDL_Delay((uint32_t)(frameDelay - frameTime));
        }
    }

    hp_app_shutdown(&app);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
