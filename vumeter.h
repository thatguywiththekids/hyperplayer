#ifndef VUMETER_H
#define VUMETER_H

#include "app.h"
#include "renderer.h"

void vumeter_update(AppState *app, double dt);
void vumeter_draw(AppState *app, HP_DrawContext *ctx);

#endif