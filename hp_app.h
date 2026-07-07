#ifndef HP_APP_H
#define HP_APP_H

#include "app.h"
#include "renderer.h"

// Keyboard codes
typedef enum {
    HP_KEY_UNKNOWN = 0,
    HP_KEY_ESCAPE,
    HP_KEY_SPACE,
    HP_KEY_LEFT,
    HP_KEY_RIGHT,
    HP_KEY_UP,
    HP_KEY_DOWN,
    HP_KEY_S,
    HP_KEY_R
} HP_KeyCode;

// Keyboard modifier flags
typedef enum {
    HP_KEYMOD_NONE = 0,
    HP_KEYMOD_CTRL = 1 << 0,
    HP_KEYMOD_ALT  = 1 << 1,
    HP_KEYMOD_SHIFT = 1 << 2
} HP_KeyModifiers;

// Mouse buttons
typedef enum {
    HP_MOUSE_BUTTON_LEFT = 1,
    HP_MOUSE_BUTTON_MIDDLE = 2,
    HP_MOUSE_BUTTON_RIGHT = 3
} HP_MouseButton;

// Lifecycle functions
bool hp_app_init(AppState *app, HP_DrawContext *ctx);
void hp_app_shutdown(AppState *app);

// Loop functions
void hp_app_update(AppState *app, double dt);
void hp_app_draw(AppState *app, HP_DrawContext *ctx, const HP_Rect *clientRect);

// Event input functions
void hp_app_on_mouse_motion(AppState *app, int x, int y);
void hp_app_on_mouse_down(AppState *app, HP_MouseButton button, int x, int y);
void hp_app_on_mouse_wheel(AppState *app, float x, float y);
void hp_app_on_key_down(AppState *app, HP_KeyCode key, uint32_t modifiers, bool *outQuit);

#endif // HP_APP_H
