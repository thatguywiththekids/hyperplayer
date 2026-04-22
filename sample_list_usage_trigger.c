#include "sample_list_usage_trigger.h"
#include "player.h"
#include "renderer.h"
#include "portability.h"
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

typedef struct HighlightBox {
    int x;
    int y;
    int w;
    int h;
} HighlightBox;

typedef struct SampleListConfig {
    float bgTransparency;
    float bgFade;
    uint64_t lastLoadTick;
    bool loaded;
} SampleListConfig;

static const HighlightBox g_boxes[31] = {
    {800, 245, 335, 23}, {800, 272, 335, 23}, {800, 299, 335, 23}, {800, 326, 335, 23},
    {800, 353, 335, 23}, {800, 380, 335, 23}, {800, 407, 335, 23}, {800, 434, 335, 23},
    {800, 461, 335, 23}, {800, 488, 335, 23}, {800, 515, 335, 23}, {800, 542, 335, 23},
    {800, 569, 335, 23}, {800, 596, 335, 23}, {800, 623, 335, 23}, {800, 650, 335, 23},
    {800, 677, 335, 23}, {800, 704, 335, 23}, {800, 731, 335, 23}, {800, 758, 335, 23},
    {800, 785, 335, 23}, {800, 812, 335, 23}, {800, 839, 335, 23}, {800, 866, 335, 23},
    {800, 893, 335, 23}, {800, 920, 335, 23}, {800, 947, 335, 23}, {800, 974, 335, 23},
    {800, 1001, 335, 23}, {800, 1028, 335, 23}, {800, 1055, 335, 23}
};

static wchar_t g_lastFile[MAX_PATH];
static SampleListConfig g_config = { 0 };

static void reset_highlights(AppState *app)
{
    if (!app) return;
    for (int i = 0; i < 31; ++i) {
        app->sampleHighlightAlpha[i] = 0.0f;
    }
}

static void sample_list_usage_trigger_load_config(AppState *app)
{
    uint64_t now = GetTickCount64();
    if (g_config.loaded && (now - g_config.lastLoadTick) < 250) return;

    g_config.bgTransparency = (float)app_ini_get_int(app, L"SAMPLELIST", L"BGTRANSPARENCY", 54) / 100.0f;
    g_config.bgFade = (float)app_ini_get_double(app, L"SAMPLELIST", L"BGFADE", 0.8333333);

    g_config.lastLoadTick = now;
    g_config.loaded = true;
}

void sample_list_usage_trigger_update(AppState *app, double dt)
{
    bool activeSamples[31] = { false };
    if (!app) return;

    sample_list_usage_trigger_load_config(app);

    if (!player_is_loaded(app)) {
        if (g_lastFile[0] != L'\0') {
            g_lastFile[0] = L'\0';
            reset_highlights(app);
        }
        return;
    }

    const wchar_t *currentFile = player_get_current_file_path(app);
    if (wcscmp(g_lastFile, currentFile) != 0) {
        wcsncpy(g_lastFile, currentFile, MAX_PATH - 1);
        g_lastFile[MAX_PATH-1] = L'\0';
        reset_highlights(app);
    }

    for (int channel = 1; channel <= 4; ++channel) {
        QuadrascopeState state;
        if (player_get_quadrascope_state(app, channel, &state)) {
            if (state.sampleIndex > 0 && state.sampleIndex <= 31) {
                if (state.vu > 0.001f || state.active || state.scopeHold > 0.0) {
                    activeSamples[state.sampleIndex - 1] = true;
                }
            }
        }
    }

    for (int i = 0; i < 31; ++i) {
        if (activeSamples[i]) {
            app->sampleHighlightAlpha[i] = g_config.bgTransparency;
        } else {
            float nextAlpha = app->sampleHighlightAlpha[i] - (float)(g_config.bgFade * dt);
            if (nextAlpha < 0.0f) nextAlpha = 0.0f;
            app->sampleHighlightAlpha[i] = nextAlpha;
        }
    }
}

void sample_list_usage_trigger_draw(AppState *app, HP_DrawContext *ctx)
{
    if (!app || !ctx) return;
    if (!g_config.loaded) sample_list_usage_trigger_load_config(app);

    for (int i = 0; i < 31; ++i) {
        float alpha = app->sampleHighlightAlpha[i];
        if (alpha > 0.0f) {
            int alphaByte = (int)(alpha * 255.0f + 0.5f);
            if (alphaByte < 0) alphaByte = 0;
            else if (alphaByte > 255) alphaByte = 255;

            const HighlightBox *box = &g_boxes[i];
            HP_Rect r = { box->x, box->y, box->w, box->h };
            hp_draw_set_color(ctx, (HP_Color){255, 255, 255, (uint8_t)alphaByte});
            hp_draw_fill_rect(ctx, &r);
        }
    }
}
