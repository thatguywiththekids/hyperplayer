#include "hp_app.h"
#include "ui.h"
#include "player.h"
#include "directory_listing.h"
#include "action_buttons.h"
#include "sample_list.h"
#include "sample_list_usage_trigger.h"
#include "sample_display.h"
#include "spectrumanalyzer.h"
#include "vumeter.h"
#include "quadrascope.h"
#include "tunnelvisualizer.h"
#include "mousecursor.h"
#include "urls.h"
#include <stdlib.h>
#include <string.h>

bool hp_app_init(AppState *app, HP_DrawContext *ctx)
{
    if (!app_init(app)) {
        return false;
    }

    // Assets need to be loaded after renderer is ready
    ui_load_assets(app, ctx);
    mousecursor_apply();
    return true;
}

void hp_app_shutdown(AppState *app)
{
    app_shutdown(app);
    mousecursor_unload();
}

void hp_app_update(AppState *app, double dt)
{
    player_update(app, dt);
    tunnelvisualizer_update(app, dt);
    spectrumanalyzer_update(app, dt);
    vumeter_update(app, dt);
    sample_display_update(app, dt);
    sample_list_usage_trigger_update(app, dt);
}

void hp_app_draw(AppState *app, HP_DrawContext *ctx, const HP_Rect *clientRect)
{
    ui_draw(app, ctx, clientRect);
}

void hp_app_on_mouse_motion(AppState *app, int x, int y)
{
    app->mouseX = x;
    app->mouseY = y;
}

void hp_app_on_mouse_down(AppState *app, HP_MouseButton button, int x, int y)
{
    if (button == HP_MOUSE_BUTTON_LEFT) {
        if (!action_buttons_mouse_down(app, x, y)) {
            if (!urls_mouse_down(app, x, y)) {
                if (!sample_list_mouse_down(app, x, y)) {
                    if (!directory_listing_mouse_down(app, x, y)) {
                        if (!tunnelvisualizer_mouse_down(app, x, y)) {
                            // Other interactions...
                        }
                    }
                }
            }
        }
    }
}

void hp_app_on_mouse_wheel(AppState *app, float x, float y)
{
    (void)x;
    if (app->showFileBrowser) {
        directory_listing_mouse_wheel(app, (int)y * 120);
    }
}

void hp_app_on_key_down(AppState *app, HP_KeyCode key, uint32_t modifiers, bool *outQuit)
{
    if (outQuit) *outQuit = false;

    switch (key) {
        case HP_KEY_ESCAPE:
            if (outQuit) *outQuit = true;
            break;
        case HP_KEY_SPACE:
            if (player_is_paused(app) || player_is_stopped(app)) {
                player_play(app);
                app->showFileBrowser = false;
            } else {
                player_pause(app);
            }
            break;
        case HP_KEY_LEFT:
            player_jump_to_order(app, -1);
            break;
        case HP_KEY_RIGHT:
            player_jump_to_order(app, 1);
            break;
        case HP_KEY_UP:
            if (directory_listing_move_to_neighbor(app, -1)) {
                player_play(app);
                app->showFileBrowser = false;
            }
            break;
        case HP_KEY_DOWN:
            if (directory_listing_move_to_neighbor(app, 1)) {
                player_play(app);
                app->showFileBrowser = false;
            }
            break;
        case HP_KEY_S:
            player_stop(app);
            break;
        case HP_KEY_R:
            if (modifiers & HP_KEYMOD_CTRL) {
                player_restart_current_order(app);
                player_play(app);
                app->showFileBrowser = false;
            }
            break;
        default:
            break;
    }
}
