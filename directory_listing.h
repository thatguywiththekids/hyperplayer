#ifndef DIRECTORY_LISTING_POSIX_H
#define DIRECTORY_LISTING_POSIX_H

#include "app.h"
#include "renderer.h"
#include <stdbool.h>

bool directory_listing_init(AppState *app, const wchar_t *rootPath);
void directory_listing_shutdown(AppState *app);
void directory_listing_draw(AppState *app, HP_DrawContext *ctx);
bool directory_listing_mouse_down(AppState *app, int x, int y);
void directory_listing_mouse_wheel(AppState *app, float y);
bool directory_listing_move_to_neighbor(AppState *app, int step);

#endif
