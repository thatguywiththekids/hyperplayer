#include "urls.h"
#include "renderer.h"
#include "portability.h"
#include <stdint.h>
#include <wchar.h>

typedef struct RectI {
    int x, y, w, h;
} RectI;

static const RectI URL_HYPERUNKNOWN = { 1374, 48, 338, 17 };
static const RectI URL_GITHUB = { 1800, 48, 105, 17 };

static bool point_in_rect(int px, int py, const RectI *r)
{
    return (px >= r->x && px <= r->x + r->w && py >= r->y && py <= r->y + r->h);
}

static bool urls_open(AppState *app, const wchar_t *url)
{
    if (!url || url[0] == L'\0') return false;

    // ShellExecuteW is shimmed in portability_sdl3.c to call SDL_OpenURL
    void* result = ShellExecuteW(
        app ? app->hwnd : NULL,
        L"open",
        url,
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if ((uintptr_t)result <= 32) {
        if (app) app_set_status(app, L"Failed to open URL: %ls", url);
        return false;
    }
    return true;
}

void urls_draw(AppState *app, HP_DrawContext *ctx)
{
    (void)app; (void)ctx;
    // URLs are part of the background image for now.
}

bool urls_mouse_down(AppState *app, int x, int y)
{
    if (!app) return false;
    if (point_in_rect(x, y, &URL_HYPERUNKNOWN)) return urls_open(app, L"http://www.hyperunknown.net");
    if (point_in_rect(x, y, &URL_GITHUB)) return urls_open(app, L"https://github.com/hyperboxing/hyperplayer");
    return false;
}
