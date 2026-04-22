#ifndef ACTION_BUTTONS_H
#define ACTION_BUTTONS_H

#include "app.h"
#include "renderer.h"
#include <stdbool.h>

void action_buttons_draw(AppState *app, HP_DrawContext *ctx);
bool action_buttons_mouse_down(AppState *app, int x, int y);

#endif