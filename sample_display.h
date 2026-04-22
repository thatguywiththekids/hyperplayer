#ifndef SAMPLE_DISPLAY_H
#define SAMPLE_DISPLAY_H

#include "app.h"
#include "renderer.h"

void sample_display_update(AppState *app, double dt);
void sample_display_draw(AppState *app, HP_DrawContext *ctx);

#endif