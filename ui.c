#include "ui.h"
#include "directory_listing.h"
#include "player.h"
#include "mousecursor.h"
#include "pattern_view.h"
#include "spectrumanalyzer.h"
#include "vumeter.h"
#include "quadrascope.h"
#include "sample_display.h"
#include "tunnelvisualizer.h"
#include "action_buttons.h"
#include "urls.h"
#include "sample_list_usage_trigger.h"
#include "sample_list.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const HP_Color COLOR_WHITE = {255, 255, 255, 255};
static const HP_Color COLOR_INFO = {187, 187, 187, 255};
static const HP_Color COLOR_SHADOW = {89, 89, 89, 255};

static const wchar_t *FONT_FACE = L"protracker.ttf";

static void ui_delete_font(HP_Font *font)
{
    if (font && *font) {
        hp_free_font(*font);
        *font = NULL;
    }
}

static HP_Font ui_make_font(const wchar_t *path, int pixelHeight, int weight)
{
    (void)weight;
    return hp_load_font(path, pixelHeight);
}

// (Image loading helper functions have been moved to the renderer backend)

void ui_draw_shadowed_text(
    HP_DrawContext *ctx,
    HP_Font font,
    const wchar_t *text,
    int x,
    int y,
    HP_Color color,
    HP_Color shadowColor,
    int shadowDx,
    int shadowDy,
    const HP_Rect *clipRect,
    uint32_t format
) {
    if (!ctx || !text) return;

    int drawX = x;
    int drawY = y;

    hp_draw_set_font(ctx, font);

    if (format & HP_TEXT_RIGHT) { // DT_RIGHT
        int tw, th;
        hp_get_text_size(ctx, text, &tw, &th);
        if (clipRect) {
            drawX = clipRect->x + clipRect->w - tw;
        }
    }

    wchar_t truncated[256];
    const wchar_t *textToDraw = text;

    if ((format & HP_TEXT_END_ELLIPSIS) && clipRect) { // DT_END_ELLIPSIS
        int tw, th;
        hp_get_text_size(ctx, text, &tw, &th);
        if (tw > clipRect->w) {
            wcsncpy(truncated, text, 255);
            truncated[255] = L'\0';
            size_t len = wcslen(truncated);
            while (len > 3) {
                truncated[len-1] = L'\0';
                truncated[len-2] = L'.';
                truncated[len-3] = L'.';
                truncated[len-4] = L'.';
                hp_get_text_size(ctx, truncated, &tw, &th);
                if (tw <= clipRect->w) break;
                len--;
            }
            textToDraw = truncated;
        }
    }

    (void)clipRect; // TODO: Implement clipping in renderer if needed

    hp_draw_set_text_color(ctx, shadowColor);
    hp_draw_text(ctx, drawX + shadowDx, drawY + shadowDy, textToDraw);

    hp_draw_set_text_color(ctx, color);
    hp_draw_text(ctx, drawX, drawY, textToDraw);
}

static void ui_draw_fallback_shell(AppState *app, HP_DrawContext *ctx, const HP_Rect *clientRect)
{
    HP_Color bgColor = {160, 160, 160, 255};
    HP_Color darkColor = {122, 122, 122, 255};
    
    hp_draw_set_color(ctx, bgColor);
    hp_draw_fill_rect(ctx, clientRect);
    
    hp_draw_set_color(ctx, darkColor);
    HP_Rect r;
    r = (HP_Rect){1363, 134, 1918-1363, 459-134}; hp_draw_fill_rect(ctx, &r);
    r = (HP_Rect){1363, 467, 1918-1363, 618-467}; hp_draw_fill_rect(ctx, &r);
    r = (HP_Rect){1363, 658, 1918-1363, 768-658}; hp_draw_fill_rect(ctx, &r);
    r = (HP_Rect){1363, 958, 1918-1363, 1047-958}; hp_draw_fill_rect(ctx, &r);
    
    ui_draw_shadowed_text(ctx, app->fonts.title, L"HYPERPLAYER", 1375, 12, COLOR_WHITE, COLOR_SHADOW, 3, 3, NULL, 0);
}

bool ui_load_assets(AppState *app, HP_DrawContext *ctx)
{
    wchar_t cursorPath[MAX_PATH];
    app->fonts.pattern = ui_make_font(L"protracker.ttf", 18, 0);
    app->fonts.sampleList = ui_make_font(L"protracker.ttf", 17, 0);
    app->fonts.info = ui_make_font(L"protracker.ttf", 25, 0);
    app->fonts.info2 = ui_make_font(L"protracker.ttf", 24, 0);
    app->fonts.dir = ui_make_font(L"protracker.ttf", 16, 0);
    app->fonts.driveButtons = ui_make_font(L"protracker.ttf", 16, 0);
    app->fonts.waveform = ui_make_font(L"protracker.ttf", 32, 0);
    app->fonts.title = ui_make_font(L"protracker.ttf", 42, 0);
    
    hp_load_texture_file(ctx, app->backgroundPath, &app->background);
    app->backgroundLoaded = (app->background.texture != NULL);

    app_join_path(cursorPath, MAX_PATH, app->exeDir, L"PT-mousecursor.png");
    mousecursor_load(cursorPath, 0, 0);

    return true;
}

void ui_release_assets(AppState *app)
{
    ui_delete_font(&app->fonts.pattern);
    ui_delete_font(&app->fonts.sampleList);
    ui_delete_font(&app->fonts.info);
    ui_delete_font(&app->fonts.info2);
    ui_delete_font(&app->fonts.dir);
    ui_delete_font(&app->fonts.driveButtons);
    ui_delete_font(&app->fonts.waveform);
    ui_delete_font(&app->fonts.title);
    hp_destroy_texture(&app->background);
}

void ui_draw(AppState *app, HP_DrawContext *ctx, const HP_Rect *clientRect)
{
    if (!app || !ctx || !clientRect) return;
    
    if (app->backgroundLoaded) {
        hp_draw_texture(ctx, &app->background, NULL, clientRect, 255);
    } else {
        ui_draw_fallback_shell(app, ctx, clientRect);
    }

    if (app->showFileBrowser) {
        directory_listing_draw(app, ctx);
    } else {
        tunnelvisualizer_draw(app, ctx);
    }

    action_buttons_draw(app, ctx);
    urls_draw(app, ctx);

    pattern_view_draw(app, ctx);
    spectrumanalyzer_draw(app, ctx);
    vumeter_draw(app, ctx);
    quadrascope_draw(app, ctx);
    sample_display_draw(app, ctx);
    sample_list_usage_trigger_draw(app, ctx);
    sample_list_draw(app, ctx);
    player_draw_songinfo(app, ctx);
}
