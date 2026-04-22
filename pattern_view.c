#include "pattern_view.h"
#include "player.h"
#include "renderer.h"
#include "portability.h"
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#define PATTERN_VISIBLE_ROWS 64

typedef struct PatternRow {
    wchar_t rowText[8];
    wchar_t cells[4][16];
} PatternRow;

typedef struct PatternCache {
    bool valid;
    PatternRow rows[PATTERN_VISIBLE_ROWS];
    int highlightRow;

    wchar_t keyFile[MAX_PATH];
    int keyOrder;
    int keyPattern;
    int keyRow;
    bool keyStopped;
    bool keyLoopEnabled;
    bool keyLocked;
} PatternCache;

typedef struct ScrollState {
    bool locked;
    wchar_t lastFile[MAX_PATH];
    bool lastStopped;
} ScrollState;

static const int g_lineXs[4] = { 73, 245, 417, 589 };
static PatternCache g_cache = { 0 };
static ScrollState g_scroll = { false, L"", true };

static const HP_Color COLOR_WHITE = {255, 255, 255, 255};

static void copy_wstr(wchar_t *dst, size_t dstCount, const wchar_t *src)
{
    if (!dst || dstCount == 0) return;
    if (!src) { dst[0] = L'\0'; return; }
    wcsncpy(dst, src, dstCount - 1);
    dst[dstCount - 1] = L'\0';
}

static void load_pattern_colors(AppState *app, HP_Color *outTextColor1, HP_Color *outTextColor2)
{
    if (!outTextColor1 || !outTextColor2) return;

    *outTextColor1 = app_ini_get_color(app, L"PATTERN", L"TEXTCOLOR1", (HP_Color){54, 72, 255, 255});
    *outTextColor2 = app_ini_get_color(app, L"PATTERN", L"TEXTCOLOR2", (HP_Color){113, 150, 255, 255});
}

static void build_empty_rows(void)
{
    for (int i = 0; i < PATTERN_VISIBLE_ROWS; ++i) {
        swprintf(g_cache.rows[i].rowText, 8, L"%02d", i);
        for (int ch = 0; ch < 4; ++ch) {
            copy_wstr(g_cache.rows[i].cells[ch], 16, L"--- 00000");
        }
    }
}

static void reset_cache_to_empty(void)
{
    g_cache.valid = true;
    g_cache.highlightRow = 1;
    g_cache.keyFile[0] = L'\0';
    g_cache.keyOrder = 0;
    g_cache.keyPattern = 0;
    g_cache.keyRow = 0;
    g_cache.keyStopped = true;
    g_cache.keyLoopEnabled = false;
    g_cache.keyLocked = false;

    g_scroll.locked = false;
    g_scroll.lastFile[0] = L'\0';
    g_scroll.lastStopped = true;

    build_empty_rows();
}

static void update_scroll_lock(const AppState *app)
{
    const wchar_t *currentFile = player_get_current_file_path(app);
    bool isStopped = player_is_stopped(app);

    if (wcscmp(g_scroll.lastFile, currentFile) != 0) {
        g_scroll.locked = false;
        copy_wstr(g_scroll.lastFile, MAX_PATH, currentFile);
    }

    if (isStopped && !g_scroll.lastStopped) g_scroll.locked = false;
    if (!isStopped && player_get_current_row(app) >= 32) g_scroll.locked = true;

    g_scroll.lastStopped = isStopped;
}

static int get_highlight_row(const AppState *app)
{
    update_scroll_lock(app);
    int row = player_get_current_row(app);
    if (player_is_stopped(app)) return (row + 1 < 33) ? row + 1 : 33;
    if (g_scroll.locked) return 33;
    return (row + 1 < 33) ? row + 1 : 33;
}

static bool resolve_song_position(const AppState *app, int rowOffset, int *outOrder, int *outPattern, int *outRow)
{
    if (!app || !outOrder || !outPattern || !outRow || !player_is_loaded(app)) return false;

    int songLength = player_get_num_orders(app);
    if (songLength <= 0) return false;

    int order = player_get_current_order(app);
    int row = player_get_current_row(app);

    while (rowOffset > 0) {
        int pattern = player_get_order_pattern(app, order);
        if (pattern < 0) return false;
        int rowCount = player_get_pattern_num_rows(app, pattern);
        row++;
        if (row >= rowCount) {
            order++;
            if (order >= songLength) {
                if (player_get_loop_enabled(app)) order = 0;
                else return false;
            }
            row = 0;
        }
        rowOffset--;
    }

    while (rowOffset < 0) {
        row--;
        if (row < 0) {
            order--;
            if (order < 0) {
                if (player_get_loop_enabled(app)) order = songLength - 1;
                else return false;
            }
            int pattern = player_get_order_pattern(app, order);
            if (pattern < 0) return false;
            int rowCount = player_get_pattern_num_rows(app, pattern);
            row = (rowCount > 0) ? (rowCount - 1) : 0;
        }
        rowOffset++;
    }

    *outPattern = player_get_order_pattern(app, order);
    if (*outPattern < 0) return false;
    *outOrder = order;
    *outRow = row;
    return true;
}

static void rebuild_cache(const AppState *app)
{
    build_empty_rows();
    int highlightRow = get_highlight_row(app);
    g_cache.highlightRow = highlightRow;

    for (int visibleRow = 1; visibleRow <= PATTERN_VISIBLE_ROWS; ++visibleRow) {
        int rowOffset = visibleRow - highlightRow;
        int order, pattern, row;

        if (resolve_song_position(app, rowOffset, &order, &pattern, &row)) {
            swprintf(g_cache.rows[visibleRow - 1].rowText, 8, L"%02d", row);
            for (int channel = 0; channel < 4; ++channel) {
                player_format_pattern_cell(app, pattern, row, channel, 
                    g_cache.rows[visibleRow - 1].cells[channel], 16);
            }
        }
    }

    copy_wstr(g_cache.keyFile, MAX_PATH, player_get_current_file_path(app));
    g_cache.keyOrder = player_get_current_order(app);
    g_cache.keyPattern = player_get_current_pattern(app);
    g_cache.keyRow = player_get_current_row(app);
    g_cache.keyStopped = player_is_stopped(app);
    g_cache.keyLoopEnabled = player_get_loop_enabled(app);
    g_cache.keyLocked = g_scroll.locked;
    g_cache.valid = true;
}

static void ensure_cache(const AppState *app)
{
    if (!player_is_loaded(app)) {
        if (!g_cache.valid) reset_cache_to_empty();
        return;
    }

    const wchar_t *currentFile = player_get_current_file_path(app);
    bool isStopped = player_is_stopped(app);
    bool loopEnabled = player_get_loop_enabled(app);
    int order = player_get_current_order(app);
    int pattern = player_get_current_pattern(app);
    int row = player_get_current_row(app);

    update_scroll_lock(app);

    if (!g_cache.valid ||
        wcscmp(g_cache.keyFile, currentFile) != 0 ||
        g_cache.keyOrder != order ||
        g_cache.keyPattern != pattern ||
        g_cache.keyRow != row ||
        g_cache.keyStopped != isStopped ||
        g_cache.keyLoopEnabled != loopEnabled ||
        g_cache.keyLocked != g_scroll.locked) {
        rebuild_cache(app);
    }
}

void pattern_view_draw(AppState *app, HP_DrawContext *ctx)
{
    if (!app || !ctx || !app->fonts.pattern) return;

    ensure_cache(app);
    HP_Color textColor1, textColor2;
    load_pattern_colors(app, &textColor1, &textColor2);

    hp_draw_set_font(ctx, app->fonts.pattern);
    bool canHighlight = player_is_loaded(app) && !player_is_stopped(app);

    for (int visibleRow = 1; visibleRow <= PATTERN_VISIBLE_ROWS; ++visibleRow) {
        int rowY = (int)(1.0 + ((visibleRow - 1) * 16.75) + 0.5);
        bool highlight = canHighlight && (visibleRow == g_cache.highlightRow);
        const PatternRow *rowData = &g_cache.rows[visibleRow - 1];
        HP_Color color = textColor1;

        if (wcscmp(rowData->rowText, L"00") == 0) color = textColor2;
        if (highlight) color = COLOR_WHITE;

        hp_draw_set_text_color(ctx, color);
        hp_draw_text(ctx, 16, rowY, rowData->rowText);

        for (int channel = 0; channel < 4; ++channel) {
            hp_draw_text(ctx, g_lineXs[channel], rowY, rowData->cells[channel]);
        }
    }
}
