#include "vumeter.h"
#include "player.h"
#include "renderer.h"
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct BoxRect {
    int x;
    int y;
    int w;
    int h;
} BoxRect;

typedef struct VuMeterConfig {
    bool loaded;
    HP_Color color1;
    HP_Color color2;
    HP_Color color3;
    uint8_t alpha;
} VuMeterConfig;

static const BoxRect g_boxes[] = {
    {1379, 816, 17, 101}, {1402, 816, 17, 101}, {1425, 816, 17, 101}, {1448, 816, 17, 101},
    {1471, 816, 17, 101}, {1494, 816, 17, 101}, {1517, 816, 17, 101}, {1540, 816, 17, 101},
    {1563, 816, 17, 101}, {1586, 816, 17, 101}, {1609, 816, 17, 101}, {1632, 816, 17, 101},
    {1655, 816, 17, 101}, {1678, 816, 17, 101}, {1701, 816, 17, 101}, {1724, 816, 17, 101},
    {1747, 816, 17, 101}, {1770, 816, 17, 101}, {1793, 816, 17, 101}, {1816, 816, 17, 101},
    {1839, 816, 17, 101}, {1862, 816, 17, 101}, {1885, 816, 17, 101}
};

static float g_level = 0.0f;

static VuMeterConfig g_config = {
    false,
    {0, 255, 0, 255},
    {255, 255, 0, 255},
    {255, 0, 0, 255},
    84
};

static void load_vumeter_config(const AppState *app)
{
    if (g_config.loaded) return;
    g_config.color1 = app_ini_get_color(app, L"VUMETER", L"VUCOLOR1", g_config.color1);
    g_config.color2 = app_ini_get_color(app, L"VUMETER", L"VUCOLOR2", g_config.color2);
    g_config.color3 = app_ini_get_color(app, L"VUMETER", L"VUCOLOR3", g_config.color3);
    g_config.alpha = (uint8_t)app_ini_get_int(app, L"VUMETER", L"VUTRANSPARENCY", g_config.alpha);
    g_config.loaded = true;
}

static HP_Color get_box_color(const AppState *app, int index, int total)
{
    float t = (float)index / (float)(total - 1);
    HP_Color c;

    if (!g_config.loaded) load_vumeter_config(app);

    if (t < 0.70f) c = g_config.color1;
    else if (t < 0.90f) c = g_config.color2;
    else c = g_config.color3;

    c.a = g_config.alpha;
    return c;
}

void vumeter_update(AppState *app, double dt)
{
    float target = 0.0f;
    (void)dt;

    if (app && player_is_loaded(app) && !player_is_paused(app) && !player_is_stopped(app)) {
        target = player_get_recent_output_level(app) * 5.0f;
        if (target > 1.0f) target = 1.0f;
    }

    if (target > g_level) g_level = g_level + ((target - g_level) * 0.35f);
    else g_level = g_level + ((target - g_level) * 0.08f);

    if (g_level < 0.0f) g_level = 0.0f;
    if (g_level > 1.0f) g_level = 1.0f;
}

void vumeter_draw(AppState *app, HP_DrawContext *ctx)
{
    int lit;
    int total;

    if (!ctx) return;
    if (!g_config.loaded) load_vumeter_config(app);

    total = (int)(sizeof(g_boxes) / sizeof(g_boxes[0]));
    lit = (int)(g_level * (float)total + 0.5f);

    for (int i = 0; i < lit && i < total; ++i) {
        const BoxRect *box = &g_boxes[i];
        HP_Color color = get_box_color(app, i, total);
        HP_Rect r = { box->x, box->y, box->w, box->h };
        hp_draw_set_color(ctx, color);
        hp_draw_fill_rect(ctx, &r);
    }
}
