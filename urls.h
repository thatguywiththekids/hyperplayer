#ifndef URLS_H
#define URLS_H

#include "app.h"
#include "renderer.h"
#include <stdbool.h>

void urls_draw(AppState *app, HP_DrawContext *ctx);
bool urls_mouse_down(AppState *app, int x, int y);

#endif