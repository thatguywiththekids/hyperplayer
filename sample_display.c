#include "sample_display.h"
#include "player.h"
#include "ui.h"
#include "renderer.h"
#include "portability.h"
#include <math.h>
#include <stdio.h>
#include <wchar.h>

static const int AREA_X = 1363;
static const int AREA_Y = 467;
static const int AREA_W = 555;
static const int AREA_H = 151;

static const HP_Color DEFAULT_COLOR_WAVEFORM = {255, 221, 0, 255};
static const HP_Color COLOR_INFO = {187, 187, 187, 255};
static const HP_Color COLOR_SHADOW = {89, 89, 89, 255};

static HP_Color g_colorWaveform = {255, 221, 0, 255};
static bool g_configLoaded = false;

static void load_sample_display_config(AppState *app)
{
    g_colorWaveform = app_ini_get_color(app, L"SAMPLEVIEW", L"SAMPLECOLOR", DEFAULT_COLOR_WAVEFORM);
    g_configLoaded = true;
}

static int get_displayed_sample_index(AppState *app)
{
    if (!app) return 0;
    if (app->selectedSampleIndex > 0) return app->selectedSampleIndex;

    int nonEmptyCount = player_get_nonempty_sample_count(app);
    if (nonEmptyCount <= 0) return 0;

    if (app->sampleDisplayCurrentSlot < 0 || app->sampleDisplayCurrentSlot >= nonEmptyCount) {
        app->sampleDisplayCurrentSlot = 0;
    }

    return player_get_nonempty_sample_index(app, app->sampleDisplayCurrentSlot);
}

void sample_display_update(AppState *app, double dt)
{
    if (!app) return;
    if (!g_configLoaded) load_sample_display_config(app);
    if (app->selectedSampleIndex > 0) return;

    int nonEmptyCount = player_get_nonempty_sample_count(app);
    if (nonEmptyCount <= 0) return;

    app->sampleDisplayTimer += dt;
    if (app->sampleDisplayTimer >= 2.0) {
        app->sampleDisplayTimer -= 2.0;
        app->sampleDisplayCurrentSlot++;
        if (app->sampleDisplayCurrentSlot >= nonEmptyCount) app->sampleDisplayCurrentSlot = 0;
    }
}

void sample_display_draw(AppState *app, HP_DrawContext *ctx)
{
    const SamplePreviewPoint *preview = NULL;
    int previewCount = 0;

    if (!app || !ctx) return;
    if (!g_configLoaded) load_sample_display_config(app);

    int sampleIndex = get_displayed_sample_index(app);
    if (sampleIndex <= 0) return;

    if (!player_get_sample_preview(app, sampleIndex, &preview, &previewCount) || !preview || previewCount <= 0) {
        return;
    }

    hp_draw_set_color(ctx, g_colorWaveform);

    for (int i = 0; i < previewCount && i < AREA_W; ++i) {
        int px = AREA_X + i;
        float minv = preview[i].minValue;
        float maxv = preview[i].maxValue;
        int midY = AREA_Y + (AREA_H / 2);
        int y1 = (int)(midY - (maxv * (AREA_H * 0.45f)));
        int y2 = (int)(midY - (minv * (AREA_H * 0.45f)));

        hp_draw_line(ctx, px, y1, px, y2);
    }

    wchar_t numText[8];
    swprintf(numText, 8, L"%02X", sampleIndex);

    HP_Rect numRect = {1831, 454, 80, 40};

    ui_draw_shadowed_text(
        ctx,
        app->fonts.waveform,
        numText,
        1831,
        454,
        COLOR_INFO,
        COLOR_SHADOW,
        2,
        2,
        &numRect,
        0x00000002 // DT_RIGHT
    );
}
