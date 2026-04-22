#ifndef UI_H
#define UI_H

#include "app.h"
#include "renderer.h"

bool ui_load_assets(AppState *app, void *renderer);
void ui_release_assets(AppState *app);
void ui_draw(AppState *app, HP_DrawContext *ctx, const HP_Rect *clientRect);

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
    UINT format
);

#endif