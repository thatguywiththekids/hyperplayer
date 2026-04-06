#include "player.h"
#include "ui.h"
#include "portability.h"
#include <libopenmpt/libopenmpt.h>
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <math.h>

#define PLAYER_SAMPLE_RATE 44100
#define PLAYER_BUFFER_FRAMES 1024
#define AUDIO_HISTORY_SIZE 131072

struct PlayerState {
    AppState *app;
    openmpt_module *mod;
    SDL_AudioStream *stream;
    
    int sampleRate;
    int bufferFrames;
    bool paused;
    bool stopped;
    bool loopEnabled;
    
    float *audioHistory;
    int audioHistoryWritePos;
    
    wchar_t currentFile[MAX_PATH];
    wchar_t currentName[256];
};

bool player_init(AppState *app) {
    PlayerState *p = (PlayerState *)calloc(1, sizeof(PlayerState));
    if (!p) return false;
    
    p->app = app;
    p->sampleRate = PLAYER_SAMPLE_RATE;
    p->bufferFrames = PLAYER_BUFFER_FRAMES;
    p->loopEnabled = true;
    
    p->audioHistory = (float *)calloc(AUDIO_HISTORY_SIZE, sizeof(float));
    
    SDL_AudioSpec spec = { SDL_AUDIO_S16LE, 2, PLAYER_SAMPLE_RATE };
    p->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!p->stream) {
        free(p->audioHistory);
        free(p);
        return false;
    }
    
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(p->stream));
    
    app->player = p;
    p->stopped = true;
    
    return true;
}

void player_shutdown(AppState *app) {
    PlayerState *p = app->player;
    if (!p) return;
    
    if (p->mod) {
        openmpt_module_destroy(p->mod);
    }
    
    SDL_DestroyAudioStream(p->stream);
    free(p->audioHistory);
    free(p);
    app->player = NULL;
}

bool player_is_available(const AppState *app) { return app->player != NULL; }
bool player_is_loaded(const AppState *app) { return app->player && app->player->mod != NULL; }
bool player_is_paused(const AppState *app) { return app->player && app->player->paused; }
bool player_is_stopped(const AppState *app) { return app->player && app->player->stopped; }

const wchar_t *player_get_current_name(const AppState *app) { return app->player ? app->player->currentName : L""; }
const wchar_t *player_get_current_file_path(const AppState *app) { return app->player ? app->player->currentFile : L""; }

int player_get_num_orders(const AppState *app) { return app->player && app->player->mod ? openmpt_module_get_num_orders(app->player->mod) : 0; }
int player_get_current_order(const AppState *app) { return app->player && app->player->mod ? openmpt_module_get_current_order(app->player->mod) : 0; }
int player_get_current_pattern(const AppState *app) { return app->player && app->player->mod ? openmpt_module_get_current_pattern(app->player->mod) : 0; }
int player_get_current_row(const AppState *app) { return app->player && app->player->mod ? openmpt_module_get_current_row(app->player->mod) : 0; }
bool player_get_loop_enabled(const AppState *app) { return app->player ? app->player->loopEnabled : false; }

int player_get_order_pattern(const AppState *app, int order) { return app->player && app->player->mod ? openmpt_module_get_order_pattern(app->player->mod, order) : 0; }
int player_get_pattern_num_rows(const AppState *app, int pattern) { return app->player && app->player->mod ? openmpt_module_get_pattern_num_rows(app->player->mod, pattern) : 0; }
int player_get_num_channels(const AppState *app) { return app->player && app->player->mod ? openmpt_module_get_num_channels(app->player->mod) : 0; }

bool player_format_pattern_cell(const AppState *app, int pattern, int row, int channel, wchar_t *outText, size_t outCount) {
    if (!app->player || !app->player->mod) return false;
    const char *text = openmpt_module_format_pattern_row_channel_command(app->player->mod, pattern, row, channel, 0);
    if (text) {
        mbstowcs(outText, text, outCount);
        openmpt_free_string(text);
        return true;
    }
    return false;
}

float player_get_recent_output_level(const AppState *app) {
    if (!app->player || !app->player->mod) return 0.0f;
    float maxL = (float)openmpt_module_get_current_channel_vu_mono(app->player->mod, 0);
    float maxR = (float)openmpt_module_get_current_channel_vu_mono(app->player->mod, 1);
    return (maxL > maxR) ? maxL : maxR;
}

bool player_get_recent_mono_window(const AppState *app, float *outSamples, int count) {
    if (!app->player || !app->player->audioHistory) return false;
    PlayerState *p = app->player;
    int start = (p->audioHistoryWritePos - count + AUDIO_HISTORY_SIZE) % AUDIO_HISTORY_SIZE;
    for (int i = 0; i < count; ++i) {
        outSamples[i] = p->audioHistory[(start + i) % AUDIO_HISTORY_SIZE];
    }
    return true;
}

bool player_get_quadrascope_state(const AppState *app, int channel1Based, QuadrascopeState *outState) {
    if (!app->player || !app->player->mod || !outState) return false;
    memset(outState, 0, sizeof(QuadrascopeState));
    outState->active = true;
    outState->vu = (float)openmpt_module_get_current_channel_vu_mono(app->player->mod, channel1Based - 1);
    return true;
}

bool player_get_sample_info(const AppState *app, int sampleIndex1Based, wchar_t *outName, size_t outNameCount, int *outVolume, int *outSize) {
    if (!app->player || !app->player->mod) return false;
    const char *name = openmpt_module_get_sample_name(app->player->mod, sampleIndex1Based - 1);
    if (name) {
        mbstowcs(outName, name, outNameCount);
        openmpt_free_string(name);
    } else {
        if (outNameCount > 0) outName[0] = L'\0';
    }
    if (outVolume) *outVolume = 64; 
    if (outSize) *outSize = 0; 
    return true;
}

bool player_get_sample_values(const AppState *app, int sampleIndex1Based, const float **outValues, int *outCount, int *outLoopStart, int *outLoopLength) {
    return false; // Still complex
}

bool player_get_sample_preview(const AppState *app, int sampleIndex1Based, const SamplePreviewPoint **outPreview, int *outCount) {
    return false; // Still complex
}

int player_get_nonempty_sample_count(const AppState *app) {
    if (!app->player || !app->player->mod) return 0;
    return openmpt_module_get_num_samples(app->player->mod);
}

int player_get_nonempty_sample_index(const AppState *app, int slotIndex) {
    return slotIndex + 1;
}

bool player_load_module(AppState *app, const wchar_t *absolutePath, const wchar_t *displayName) {
    PlayerState *p = app->player;
    if (!p) return false;
    
    char path[MAX_PATH*4];
    wcstombs(path, absolutePath, sizeof(path));
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    void *data = malloc(size);
    if (!data) {
        fclose(f);
        return false;
    }
    
    fread(data, 1, size, f);
    fclose(f);
    
    if (p->mod) {
        openmpt_module_destroy(p->mod);
    }
    
    p->mod = openmpt_module_create_from_memory2(data, size, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    free(data);
    
    if (!p->mod) return false;
    
    const char *title = openmpt_module_get_metadata(p->mod, "title");
    if (title && title[0]) {
        mbstowcs(p->currentName, title, 256);
    } else {
        wcsncpy(p->currentName, displayName, 256);
    }
    wcsncpy(p->currentFile, absolutePath, MAX_PATH);
    
    openmpt_module_set_repeat_count(p->mod, p->loopEnabled ? -1 : 0);
    
    SDL_ClearAudioStream(p->stream);
    p->stopped = false;
    p->paused = false;
    
    return true;
}

void player_draw_songinfo(AppState *app, HDC hdc) {
    if (!app->player) return;
    
    PlayerState *p = app->player;
    wchar_t info[512];
    swprintf(info, 512, L"Module: %ls", p->currentName);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutW(hdc, 1375, 140, info, (int)wcslen(info));
    
    if (p->mod) {
        int order = openmpt_module_get_current_order(p->mod);
        int pattern = openmpt_module_get_current_pattern(p->mod);
        int row = openmpt_module_get_current_row(p->mod);
        swprintf(info, 512, L"Pos: %03d / %03d Row: %02d", order, pattern, row);
        TextOutW(hdc, 1375, 165, info, (int)wcslen(info));
    }
}

void player_play(AppState *app) { if (app->player) app->player->paused = false; }
void player_pause(AppState *app) { if (app->player) app->player->paused = true; }
void player_stop(AppState *app) { if (app->player) app->player->stopped = true; }

bool player_jump_to_order(AppState *app, int step) {
    if (!app->player || !app->player->mod) return false;
    int current = openmpt_module_get_current_order(app->player->mod);
    openmpt_module_set_position_order_row(app->player->mod, current + step, 0);
    return true;
}

bool player_restart_current_order(AppState *app) {
    if (!app->player || !app->player->mod) return false;
    int current = openmpt_module_get_current_order(app->player->mod);
    openmpt_module_set_position_order_row(app->player->mod, current, 0);
    return true;
}

void player_update(AppState *app, double dt) {
    PlayerState *p = app->player;
    if (!p || !p->mod || p->paused || p->stopped) return;
    
    int targetBytes = (int)(PLAYER_SAMPLE_RATE * 0.1 * 2 * sizeof(int16_t));
    int currentBytes = SDL_GetAudioStreamQueued(p->stream);
    
    while (currentBytes < targetBytes) {
        int16_t buffer[PLAYER_BUFFER_FRAMES * 2];
        size_t read = openmpt_module_read_interleaved_stereo(p->mod, p->sampleRate, PLAYER_BUFFER_FRAMES, buffer);
        
        if (read > 0) {
            SDL_PutAudioStreamData(p->stream, buffer, (int)(read * 2 * sizeof(int16_t)));
            currentBytes += (int)(read * 2 * sizeof(int16_t));
            
            for (size_t i = 0; i < read; ++i) {
                float s = (float)buffer[i * 2] / 32768.0f;
                p->audioHistory[p->audioHistoryWritePos] = s;
                p->audioHistoryWritePos = (p->audioHistoryWritePos + 1) % AUDIO_HISTORY_SIZE;
            }
        } else {
            if (!p->loopEnabled) {
                p->stopped = true;
            }
            break;
        }
    }
}
