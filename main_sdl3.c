#include "app.h"
#include "ui.h"
#include "player.h"
#include "directory_listing_posix.h"
#include "action_buttons.h"
#include "sample_list.h"
#include "sample_list_usage_trigger.h"
#include "sample_display.h"
#include "spectrumanalyzer.h"
#include "vumeter.h"
#include "quadrascope.h"
#include "tunnelvisualizer.h"
#include "mousecursor.h"
#include "urls.h"

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

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
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

    HDC screenHdc = GetDC(NULL);
    screenHdc->renderer = g_renderer;

    // App Initialization
    if (!app_init(&app)) {
        fprintf(stderr, "app_init failed\n");
        return 1;
    }

    app.hwnd = (HWND)g_window;

    struct HDC_REC hdc_rec = {0};
    hdc_rec.renderer = g_renderer;

    SDL_Event event;
    uint64_t lastTime = SDL_GetTicksNS();
    const int targetFps = 60;
    const int frameDelay = 1000 / targetFps;
    
    while (g_running) {
        uint32_t frameStart = SDL_GetTicks();
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
                    app.mouseX = (int)event.motion.x;
                    app.mouseY = (int)event.motion.y;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (!action_buttons_mouse_down(&app, (int)event.button.x, (int)event.button.y)) {
                            if (!urls_mouse_down(&app, (int)event.button.x, (int)event.button.y)) {
                                if (!sample_list_mouse_down(&app, (int)event.button.x, (int)event.button.y)) {
                                    if (!directory_listing_mouse_down(&app, (int)event.button.x, (int)event.button.y)) {
                                        if (!tunnelvisualizer_mouse_down(&app, (int)event.button.x, (int)event.button.y)) {
                                            // Other interactions...
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    if (app.showFileBrowser) {
                        directory_listing_mouse_wheel(&app, (int)event.wheel.y * 120);
                    }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    switch (event.key.key) {
                        case SDLK_ESCAPE:
                            g_running = false;
                            break;
                        case SDLK_SPACE:
                            if (player_is_paused(&app) || player_is_stopped(&app)) {
                                player_play(&app);
                                app.showFileBrowser = false;
                            } else {
                                player_pause(&app);
                            }
                            break;
                        case SDLK_LEFT:
                            player_jump_to_order(&app, -1);
                            break;
                        case SDLK_RIGHT:
                            player_jump_to_order(&app, 1);
                            break;
                        case SDLK_UP:
                            if (directory_listing_move_to_neighbor(&app, -1)) {
                                player_play(&app);
                                app.showFileBrowser = false;
                            }
                            break;
                        case SDLK_DOWN:
                            if (directory_listing_move_to_neighbor(&app, 1)) {
                                player_play(&app);
                                app.showFileBrowser = false;
                            }
                            break;
                        case SDLK_S:
                            player_stop(&app);
                            break;
                        case SDLK_R:
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                player_restart_current_order(&app);
                                player_play(&app);
                                app.showFileBrowser = false;
                            }
                            break;
                    }
                    break;
            }
        }

        player_update(&app, dt);
        tunnelvisualizer_update(&app, dt);
        spectrumanalyzer_update(&app, dt);
        vumeter_update(&app, dt);
        sample_display_update(&app, dt);
        sample_list_usage_trigger_update(&app, dt);

        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);

        RECT clientRect = { 0, 0, 1920, 1080 };
        ui_draw(&app, &hdc_rec, &clientRect);

        SDL_RenderPresent(g_renderer);

        uint32_t frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > frameTime) {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    app_shutdown(&app);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
