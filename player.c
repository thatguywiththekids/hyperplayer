#include "player.h"
#include "ui.h"
#include "portability.h"
#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_ext.h>
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

static const HP_Color COLOR_INFO = {0xBB, 0xBB, 0xBB, 255};

#define SAMPLE_PREVIEW_WIDTH 555

typedef struct SampleCache {
    float *values;
    int count;
    int loopStart;
    int loopLength;
    int volume;
    int size;
    wchar_t name[64];
    SamplePreviewPoint preview[SAMPLE_PREVIEW_WIDTH];
    int previewCount;
} SampleCache;

static float signed8_to_float(unsigned char byteValue) {
    if (byteValue > 127) return ((float)((int)byteValue - 256)) / 128.0f;
    return ((float)byteValue) / 128.0f;
}

static void build_sample_preview(const float *sampleValues, int sampleLength, SamplePreviewPoint *outPreview, int *outCount) {
    if (!sampleValues || sampleLength <= 0 || !outPreview || !outCount) return;
    int width = SAMPLE_PREVIEW_WIDTH;
    for (int i = 0; i < width; ++i) {
        int startIndex = (i * sampleLength) / width;
        int endIndex = (((i + 1) * sampleLength) / width) - 1;
        if (endIndex < startIndex) endIndex = startIndex;
        if (endIndex >= sampleLength) endIndex = sampleLength - 1;
        float minv = 1.0f, maxv = -1.0f;
        for (int j = startIndex; j <= endIndex; ++j) {
            float v = sampleValues[j];
            if (v < minv) minv = v;
            if (v > maxv) maxv = v;
        }
        outPreview[i].minValue = minv;
        outPreview[i].maxValue = maxv;
    }
    *outCount = width;
}

static unsigned short u16be(const unsigned char *bytes, size_t index, size_t size) {
    if (!bytes || index + 1 >= size) return 0;
    return (unsigned short)(((unsigned short)bytes[index] << 8) | (unsigned short)bytes[index + 1]);
}

static bool has_known_31_sample_signature(const unsigned char *bytes, size_t size) {
    if (!bytes || size < 1084) return false;
    const unsigned char *sig = bytes + 1080;

    // Classic signatures
    if (memcmp(sig, "M.K.", 4) == 0 || memcmp(sig, "M!K!", 4) == 0 || memcmp(sig, "N.T.", 4) == 0) return true;
    
    // Multi-channel (4CHN, 6CHN, 8CHN, etc.)
    if (isdigit(sig[0]) && memcmp(sig + 1, "CHN", 3) == 0) return true;
    if (isdigit(sig[0]) && isdigit(sig[1]) && memcmp(sig + 2, "CH", 2) == 0) return true;
    
    // StarTrekker
    if (memcmp(sig, "FLT", 3) == 0 && isdigit(sig[3])) return true;
    if (memcmp(sig, "EXO", 3) == 0 && isdigit(sig[3])) return true;
    
    // OctALyzer / OKTALYSER
    if (memcmp(sig, "CD81", 4) == 0 || memcmp(sig, "OKTA", 4) == 0) return true;
    
    // TakeTracker
    if (isdigit(sig[0]) && sig[1] == 'T' && sig[2] == 'D' && sig[3] == 'Z') return true;

    return false;
}

static void trim_sample_name_bytes(const unsigned char *src, size_t len, wchar_t *dst, size_t dstCount) {
    if (!dst || dstCount == 0) return;
    dst[0] = L'\0';
    if (!src) return;
    char temp[64];
    size_t ti = 0;
    for (size_t i = 0; i < len && ti < 63; ++i) {
        if (src[i] >= 32 && src[i] < 127) temp[ti++] = (char)src[i];
    }
    temp[ti] = '\0';
    mbstowcs(dst, temp, dstCount);
}

struct PlayerState {
    AppState *app;
    openmpt_module *mod;
    openmpt_module_ext *mod_ext;
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
    char modType[32];
    float recentOutputLevel;

    SampleCache *sampleCache;
    int sampleCacheCount;

    int lastPattern;
    int lastRow;
    QuadrascopeState channelStates[4];
};

static void clear_sample_cache(PlayerState *p);
static void ensure_sample_cache(PlayerState *p);

static const char *g_notes[12] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};

static double note_to_hz(const char *note) {
    char name[3];
    int octave = 0;
    int idx = -1;
    int midi;

    if (!note || strcmp(note, "---") == 0) return 0.0;

    name[0] = note[0];
    name[1] = note[1];
    name[2] = '\0';

    if (!isdigit((unsigned char)note[2])) return 0.0;
    octave = note[2] - '0';

    for (int i = 0; i < 12; ++i) {
        if (strcmp(g_notes[i], name) == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0) return 0.0;

    midi = (octave + 1) * 12 + idx;
    return 440.0 * pow(2.0, ((double)midi - 69.0) / 12.0);
}

static void update_channel_state_from_row(PlayerState *p) {
    if (!p || !p->mod) return;
    int pattern = openmpt_module_get_current_pattern(p->mod);
    int row = openmpt_module_get_current_row(p->mod);
    int numChannels = openmpt_module_get_num_channels(p->mod);
    
    for (int channel = 0; channel < 4; ++channel) {
        QuadrascopeState *state = &p->channelStates[channel];
        if (channel >= numChannels) {
            state->active = false;
            continue;
        }

        const char *sNote = openmpt_module_format_pattern_row_channel_command(p->mod, pattern, row, channel, 0);
        const char *sInstr = openmpt_module_format_pattern_row_channel_command(p->mod, pattern, row, channel, 1);
        
        int sampleIndex = 0;
        if (sInstr && sInstr[0] != '.' && strcmp(sInstr, "00") != 0) {
            sampleIndex = (int)strtol(sInstr, NULL, 16);
        }

        if (sampleIndex > 0) {
            state->sampleIndex = sampleIndex;
            state->active = true;
            state->scopeHold = 0.20;
            state->sampleVolume = 1.0f;
            state->samplePos = 1.0;
            state->scopeStride = 1;
        }

        if (sNote && sNote[0] != '.' && sNote[0] != '-') {
            double freq = note_to_hz(sNote);
            if (freq > 0.0) {
                state->frequency = freq;
                state->samplePos = 1.0;
                state->active = true;
                state->scopeHold = 0.20;
            }
        }

        openmpt_free_string(sNote);
        openmpt_free_string(sInstr);
    }
}

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
    
    if (p->mod_ext) {
        openmpt_module_ext_destroy(p->mod_ext);
        p->mod_ext = NULL;
        p->mod = NULL;
    }
    
    clear_sample_cache(p);
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

static void compact_command(const char *src, char *dst, size_t dstSize, int width, char emptyChar) {
    if (!src || src[0] == '\0') {
        for (int i = 0; i < width; ++i) dst[i] = emptyChar;
        dst[width] = '\0';
        return;
    }
    int j = 0;
    for (int i = 0; src[i] && j < width; ++i) {
        if (src[i] != ' ' && src[i] != '.') {
            dst[j++] = src[i];
        } else {
            dst[j++] = emptyChar;
        }
    }
    while (j < width) dst[j++] = emptyChar;
    dst[width] = '\0';
}

bool player_format_pattern_cell(const AppState *app, int pattern, int row, int channel, wchar_t *outText, size_t outCount) {
    if (!app->player || !app->player->mod) return false;
    
    char note[16], instr[16], effect[16], param[16];
    char combined[32];
    
    const char *sNote = openmpt_module_format_pattern_row_channel_command(app->player->mod, pattern, row, channel, 0);
    const char *sInstr = openmpt_module_format_pattern_row_channel_command(app->player->mod, pattern, row, channel, 1);
    const char *sEffect = openmpt_module_format_pattern_row_channel_command(app->player->mod, pattern, row, channel, 3);
    const char *sParam = openmpt_module_format_pattern_row_channel_command(app->player->mod, pattern, row, channel, 5);
    
    compact_command(sNote, note, 16, 3, '-');
    compact_command(sInstr, instr, 16, 2, '0');
    compact_command(sEffect, effect, 16, 1, '0');
    compact_command(sParam, param, 16, 2, '0');
    
    // Apply octave shift for MOD files
    if (note[0] != '-' && note[2] >= '0' && note[2] <= '9') {
        if (strcmp(app->player->modType, "mod") == 0) {
            int octave = note[2] - '0';
            octave += app->config.modOctaveOffset;
            if (octave < 0) octave = 0;
            if (octave > 9) octave = 9;
            note[2] = '0' + octave;
        }
    }
    
    openmpt_free_string(sNote);
    openmpt_free_string(sInstr);
    openmpt_free_string(sEffect);
    openmpt_free_string(sParam);
    
    snprintf(combined, sizeof(combined), "%s %s%s%s", note, instr, effect, param);
    mbstowcs(outText, combined, outCount);
    
    return true;
}

float player_get_recent_output_level(const AppState *app) {
    if (!app->player) return 0.0f;
    return app->player->recentOutputLevel;
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
    int idx = channel1Based - 1;
    if (idx < 0 || idx >= 4) return false;
    *outState = app->player->channelStates[idx];
    int numChannels = openmpt_module_get_num_channels(app->player->mod);
    if (idx < numChannels) {
        outState->vu = (float)openmpt_module_get_current_channel_vu_mono(app->player->mod, idx);
    } else {
        outState->vu = 0.0f;
    }
    return true;
}

bool player_get_sample_info(const AppState *app, int sampleIndex1Based, wchar_t *outName, size_t outNameCount, int *outVolume, int *outSize) {
    if (!app->player || !app->player->mod || sampleIndex1Based <= 0) return false;
    PlayerState *p = app->player;
    ensure_sample_cache(p);
    
    int idx = sampleIndex1Based - 1;
    if (idx >= p->sampleCacheCount) return false;
    
    SampleCache *sc = &p->sampleCache[idx];
    if (outName) wcsncpy(outName, sc->name, outNameCount);
    if (outVolume) *outVolume = sc->volume;
    if (outSize) *outSize = sc->size;
    return true;
}

static void clear_sample_cache(PlayerState *p) {
    if (!p->sampleCache) return;
    for (int i = 0; i < p->sampleCacheCount; ++i) {
        if (p->sampleCache[i].values) free(p->sampleCache[i].values);
    }
    free(p->sampleCache);
    p->sampleCache = NULL;
    p->sampleCacheCount = 0;
}

static void parse_mod_samples(PlayerState *p, const unsigned char *data, size_t size) {
    if (!p || !data || size < 600) return;
    int sampleCount = has_known_31_sample_signature(data, size) ? 31 : 15;
    int channels = openmpt_module_get_num_channels(p->mod);
    if (channels < 1) channels = 4; // Fallback
    
    size_t ordersOffset = (sampleCount == 31) ? 952 : 472;
    size_t patternsOffset = (sampleCount == 31) ? 1084 : 600;
    int highestPattern = 0;
    for (int i = 0; i < 128; ++i) {
        size_t off = ordersOffset + i;
        if (off >= size) break;
        if (data[off] > highestPattern) highestPattern = data[off];
    }
    size_t sampleDataOffset = patternsOffset + (size_t)(highestPattern + 1) * 64 * channels * 4;
    size_t cursor = sampleDataOffset;

    ensure_sample_cache(p);
    for (int i = 0; i < sampleCount && i < p->sampleCacheCount; ++i) {
        size_t base = 20 + (size_t)i * 30;
        if (base + 29 >= size) break;
        SampleCache *sc = &p->sampleCache[i];
        trim_sample_name_bytes(data + base, 22, sc->name, 64);
        sc->size = (int)u16be(data, base + 22, size) * 2;
        sc->volume = (int)data[base + 25];
        sc->loopStart = (int)u16be(data, base + 26, size) * 2;
        sc->loopLength = (int)u16be(data, base + 28, size) * 2;

        if (sc->size > 0 && cursor + sc->size <= size) {
            sc->values = (float *)malloc(sc->size * sizeof(float));
            if (sc->values) {
                for (int j = 0; j < sc->size; ++j) {
                    sc->values[j] = signed8_to_float(data[cursor + j]);
                }
                sc->count = sc->size;
                build_sample_preview(sc->values, sc->count, sc->preview, &sc->previewCount);
            }
            cursor += sc->size;
        }
    }
}

static void ensure_sample_cache(PlayerState *p) {
    if (p->sampleCache) return;
    int numSamples = openmpt_module_get_num_samples(p->mod);
    p->sampleCache = (SampleCache *)calloc(numSamples, sizeof(SampleCache));
    p->sampleCacheCount = numSamples;
    
    for (int i = 0; i < numSamples; ++i) {
        SampleCache *sc = &p->sampleCache[i];
        sc->volume = 64; // Default
        sc->previewCount = 0;
        const char *name = openmpt_module_get_sample_name(p->mod, i);
        if (name) {
            mbstowcs(sc->name, name, 64);
            openmpt_free_string(name);
        }
    }
}

bool player_get_sample_values(const AppState *app, int sampleIndex1Based, const float **outValues, int *outCount, int *outLoopStart, int *outLoopLength) {
    if (!app->player || !app->player->mod || sampleIndex1Based <= 0) return false;
    PlayerState *p = app->player;
    ensure_sample_cache(p);
    
    int idx = sampleIndex1Based - 1;
    if (idx >= p->sampleCacheCount) return false;
    
    SampleCache *sc = &p->sampleCache[idx];
    if (!sc->values) {
        // Extract using libopenmpt_ext
        openmpt_module_ext_interface_interactive *interactive = NULL;
        if (p->mod_ext && openmpt_module_ext_get_interface(p->mod_ext, LIBOPENMPT_EXT_C_INTERFACE_INTERACTIVE, &interactive, sizeof(interactive)) && interactive) {
            // Note: interactive extraction not fully implemented here yet
            // but we have it for MODs via parse_mod_samples
        }
    }
    
    if (sc->values) {
        if (outValues) *outValues = sc->values;
        if (outCount) *outCount = sc->count;
        if (outLoopStart) *outLoopStart = sc->loopStart;
        if (outLoopLength) *outLoopLength = sc->loopLength;
        return true;
    }
    
    return false;
}

bool player_get_sample_preview(const AppState *app, int sampleIndex1Based, const SamplePreviewPoint **outPreview, int *outCount) {
    if (!app->player || !app->player->mod || sampleIndex1Based <= 0) return false;
    PlayerState *p = app->player;
    ensure_sample_cache(p);
    int idx = sampleIndex1Based - 1;
    if (idx >= p->sampleCacheCount) return false;
    SampleCache *sc = &p->sampleCache[idx];
    if (sc->previewCount > 0) {
        if (outPreview) *outPreview = sc->preview;
        if (outCount) *outCount = sc->previewCount;
        return true;
    }
    return false;
}

static bool is_sample_non_empty(const SampleCache *sc) {
    if (!sc) return false;
    // Only consider it "non-empty" for the display cycle if it has actual waveform data
    return (sc->previewCount > 0);
}

int player_get_nonempty_sample_count(const AppState *app) {
    if (!app->player || !app->player->mod) return 0;
    PlayerState *p = app->player;
    ensure_sample_cache(p);
    
    int count = 0;
    for (int i = 0; i < p->sampleCacheCount; ++i) {
        if (is_sample_non_empty(&p->sampleCache[i])) {
            count++;
        }
    }
    return count;
}

int player_get_nonempty_sample_index(const AppState *app, int slotIndex) {
    if (!app->player || !app->player->mod) return 0;
    PlayerState *p = app->player;
    ensure_sample_cache(p);

    int currentSlot = 0;
    for (int i = 0; i < p->sampleCacheCount; ++i) {
        if (is_sample_non_empty(&p->sampleCache[i])) {
            if (currentSlot == slotIndex) {
                return i + 1; // 1-based index
            }
            currentSlot++;
        }
    }
    return 0;
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
    
    if (p->mod_ext) {
        openmpt_module_ext_destroy(p->mod_ext);
        p->mod_ext = NULL;
        p->mod = NULL;
    }
    
    p->mod_ext = openmpt_module_ext_create_from_memory(data, size, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (!p->mod_ext) {
        free(data);
        return false;
    }
    
    p->mod = openmpt_module_ext_get_module(p->mod_ext);
    
    // Store module type for format-specific logic
    const char *type = openmpt_module_get_metadata(p->mod, "type");
    if (type) {
        strncpy(p->modType, type, 31);
        p->modType[31] = '\0';
        openmpt_free_string(type);
    } else {
        p->modType[0] = '\0';
    }
    
    // Clear cache from previous module
    clear_sample_cache(p);
    
    // Parse MOD metadata manually for restoration (only for classic MODs)
    if (strcmp(p->modType, "mod") == 0) {
        parse_mod_samples(p, (const unsigned char *)data, size);
    }
    
    free(data);
    
    if (!p->mod_ext) return false;
    
    // Attempt to get module title
    const char *title = openmpt_module_get_metadata(p->mod, "title");
    if (title && title[0]) {
        if (mbstowcs(p->currentName, title, 256) == (size_t)-1) {
            wcsncpy(p->currentName, L"Unknown Module", 256);
        }
    } else {
        if (displayName && displayName[0]) {
            wcsncpy(p->currentName, displayName, 256);
        } else {
            wcsncpy(p->currentName, L"No Title", 256);
        }
    }
    wcsncpy(p->currentFile, absolutePath, MAX_PATH);
    
    openmpt_module_set_repeat_count(p->mod, p->loopEnabled ? -1 : 0);
    
    SDL_ClearAudioStream(p->stream);
    p->stopped = false;
    p->paused = false;

    p->lastRow = -1;
    p->lastPattern = -1;
    memset(p->channelStates, 0, sizeof(p->channelStates));
    
    return true;
}

void player_draw_songinfo(AppState *app, HP_DrawContext *ctx) {
    PlayerState *p;
    wchar_t posText[32];
    wchar_t posSuffix[32];
    wchar_t patternText[32];
    wchar_t lengthText[32];
    wchar_t bpmText[32];
    wchar_t speedText[32];
    int currentPosW, currentPosH;
    const wchar_t *title;

    if (!app || !app->player || !ctx) {
        return;
    }

    p = app->player;
    if (!p->mod) {
        return;
    }

    title = p->currentName;

    int order = openmpt_module_get_current_order(p->mod);
    int numOrders = openmpt_module_get_num_orders(p->mod);
    int pattern = openmpt_module_get_current_pattern(p->mod);
    int bpm = (int)(openmpt_module_get_current_tempo2(p->mod) + 0.5);
    int speed = openmpt_module_get_current_speed(p->mod);

    swprintf(posText, 32, L"%02d", order);
    swprintf(posSuffix, 32, L"/%02d", numOrders);
    swprintf(patternText, 32, L"%02d", pattern);
    swprintf(lengthText, 32, L"%02d", numOrders);
    swprintf(bpmText, 32, L"%03d", bpm);
    swprintf(speedText, 32, L"%02d", speed);

    HP_Color white = {255, 255, 255, 255};
    HP_Color shadow = {89, 89, 89, 255};
    HP_Color info = {187, 187, 187, 255};

    ui_draw_shadowed_text(ctx, app->fonts.info, title, 929, 3, white, shadow, 3, 3, NULL, 0);

    hp_draw_set_font(ctx, app->fonts.info2);
    hp_get_text_size(ctx, posText, &currentPosW, &currentPosH);

    ui_draw_shadowed_text(ctx, app->fonts.info2, posText, 931, 44, white, shadow, 3, 3, NULL, 0);
    ui_draw_shadowed_text(ctx, app->fonts.info2, posSuffix, 931 + currentPosW, 44, info, shadow, 3, 3, NULL, 0);

    ui_draw_shadowed_text(ctx, app->fonts.info2, patternText, 931, 74, white, shadow, 3, 3, NULL, 0);
    ui_draw_shadowed_text(ctx, app->fonts.info2, lengthText, 931, 104, white, shadow, 3, 3, NULL, 0);
    ui_draw_shadowed_text(ctx, app->fonts.info2, bpmText, 931, 134, white, shadow, 3, 3, NULL, 0);
    ui_draw_shadowed_text(ctx, app->fonts.info2, speedText, 931, 164, white, shadow, 3, 3, NULL, 0);
}

void player_play(AppState *app) {
    if (app->player) {
        app->player->paused = false;
        app->player->stopped = false;
    }
}

void player_pause(AppState *app) {
    if (app->player) {
        app->player->paused = true;
    }
}

void player_stop(AppState *app) {
    if (app->player) {
        app->player->stopped = true;
        app->player->paused = false;
        if (app->player->mod) {
            openmpt_module_set_position_seconds(app->player->mod, 0.0);
        }
        SDL_ClearAudioStream(app->player->stream);
        app->player->lastRow = -1;
        app->player->lastPattern = -1;
        memset(app->player->channelStates, 0, sizeof(app->player->channelStates));
    }
}

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

#define SCOPE_ADVANCE_SCALE 16.0

void player_update(AppState *app, double dt) {
    PlayerState *p = app->player;
    if (!p || !p->mod || p->paused || p->stopped) return;

    int curPattern = openmpt_module_get_current_pattern(p->mod);
    int curRow = openmpt_module_get_current_row(p->mod);

    if (curPattern != p->lastPattern || curRow != p->lastRow) {
        update_channel_state_from_row(p);
        p->lastPattern = curPattern;
        p->lastRow = curRow;
    }

    for (int i = 0; i < 4; i++) {
        QuadrascopeState *state = &p->channelStates[i];
        if (state->scopeHold > 0.0) {
            state->scopeHold -= dt;
            if (state->scopeHold < 0.0) state->scopeHold = 0.0;
        }

        float vu = (float)openmpt_module_get_current_channel_vu_mono(p->mod, i);
        if (vu < 0.001f && state->scopeHold <= 0.0) {
            state->active = false;
        }

        if (state->active && state->frequency > 0.0) {
            state->samplePos += state->frequency * dt * SCOPE_ADVANCE_SCALE;
        }
    }
    
    int targetBytes = (int)(PLAYER_SAMPLE_RATE * 0.1 * 2 * sizeof(int16_t));
    int currentBytes = SDL_GetAudioStreamQueued(p->stream);
    
    while (currentBytes < targetBytes) {
        int16_t buffer[PLAYER_BUFFER_FRAMES * 2];
        size_t read = openmpt_module_read_interleaved_stereo(p->mod, p->sampleRate, PLAYER_BUFFER_FRAMES, buffer);
        
        if (read > 0) {
            SDL_PutAudioStreamData(p->stream, buffer, (int)(read * 2 * sizeof(int16_t)));
            currentBytes += (int)(read * 2 * sizeof(int16_t));
            
            float accum = 0.0f;
            for (size_t i = 0; i < read; ++i) {
                float l = (float)buffer[i * 2] / 32768.0f;
                float r = (float)buffer[i * 2 + 1] / 32768.0f;
                float mono = (l + r) * 0.5f;
                p->audioHistory[p->audioHistoryWritePos] = mono;
                p->audioHistoryWritePos = (p->audioHistoryWritePos + 1) % AUDIO_HISTORY_SIZE;
                accum += (mono < 0.0f) ? -mono : mono;
            }
            if (read > 0) {
                p->recentOutputLevel = accum / (float)read;
            }
        } else {
            if (!p->loopEnabled) {
                p->stopped = true;
            }
            break;
        }
    }
}
