#ifndef SAMPLE_LIST_H
#define SAMPLE_LIST_H

#include "app.h"
#include "renderer.h"
#include <stdbool.h>

void sample_list_draw(AppState *app, HP_DrawContext *ctx);
bool sample_list_mouse_down(AppState *app, int x, int y);

#endif