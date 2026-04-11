#include "ui.h"
#ifdef _WIN32
#include "directory_listing_win32.h"
#else
#include "directory_listing_posix.h"
#endif
#include "player.h"
#include "mousecursor.h"
#include "pattern_view.h"
#include "spectrumanalyzer.h"
#include "vumeter.h"
#include "quadrascope.h"
#include "sample_display.h"
#include "tunnelvisualizer.h"
#include "sample_list_usage_trigger.h"
#include "sample_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const COLORREF COLOR_WHITE = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF COLOR_INFO = RGB(0xBB, 0xBB, 0xBB);
static const COLORREF COLOR_SHADOW = RGB(0x59, 0x59, 0x59);

static const wchar_t *FONT_FACE = L"protracker-fix";

static void ui_delete_font(HFONT *font)
{
    if (font && *font) {
        DeleteObject(*font);
        *font = NULL;
    }
}

static HFONT ui_make_font(const wchar_t *faceName, int pixelHeight, int weight)
{
    return CreateFontW(
        -pixelHeight,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,
        FF_DONTCARE,
        faceName
    );
}

static bool ui_load_image_portable(void *renderer, const wchar_t *path, ImageRGBA *outImage)
{
    char mbsPath[MAX_PATH*4];
    wcstombs(mbsPath, path, sizeof(mbsPath));
    
    SDL_Surface *surface = IMG_Load(mbsPath);
    if (!surface) return false;
    
    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_BGRA32);
    SDL_DestroySurface(surface);
    if (!converted) return false;
    
    outImage->width = converted->w;
    outImage->height = converted->h;
    outImage->pixels = (unsigned char *)malloc(converted->w * converted->h * 4);
    if (outImage->pixels) {
        memcpy(outImage->pixels, converted->pixels, converted->w * converted->h * 4);
        outImage->gpuTexture = SDL_CreateTextureFromSurface((SDL_Renderer *)renderer, converted);
    }
    
    SDL_DestroySurface(converted);
    return outImage->pixels != NULL;
}

static void ui_free_image(ImageRGBA *image)
{
    if (!image) return;
    if (image->pixels) free(image->pixels);
    if (image->gpuTexture) SDL_DestroyTexture((SDL_Texture *)image->gpuTexture);
    image->pixels = NULL;
    image->gpuTexture = NULL;
    image->width = 0;
    image->height = 0;
}

static void ui_draw_image(HDC hdc, const RECT *clientRect, const ImageRGBA *image)
{
    if (!hdc || !clientRect || !image) return;

    if (image->gpuTexture) {
        SDL_FRect dst = { (float)clientRect->left, (float)clientRect->top, (float)(clientRect->right - clientRect->left), (float)(clientRect->bottom - clientRect->top) };
        SDL_RenderTexture(hdc->renderer, (SDL_Texture *)image->gpuTexture, NULL, &dst);
        return;
    }

    if (!image->pixels) return;
    
    SDL_Surface *surface = SDL_CreateSurfaceFrom(image->width, image->height, SDL_PIXELFORMAT_BGRA32, image->pixels, image->width * 4);
    if (!surface) return;
    
    SDL_Texture *texture = SDL_CreateTextureFromSurface(hdc->renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) return;
    
    SDL_FRect dst = { (float)clientRect->left, (float)clientRect->top, (float)(clientRect->right - clientRect->left), (float)(clientRect->bottom - clientRect->top) };
    SDL_RenderTexture(hdc->renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

void ui_draw_shadowed_text(
    HDC hdc,
    HFONT font,
    const wchar_t *text,
    int x,
    int y,
    COLORREF color,
    COLORREF shadowColor,
    int shadowDx,
    int shadowDy,
    const RECT *clipRect,
    UINT format
) {
    RECT shadowRect;
    RECT textRect;
    int savedDc;

    if (!hdc || !text) return;

    savedDc = SaveDC(hdc);
    SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    if (clipRect) IntersectClipRect(hdc, clipRect->left, clipRect->top, clipRect->right, clipRect->bottom);

    SetTextColor(hdc, shadowColor);
    if (format != 0) {
        shadowRect.left = x + shadowDx;
        shadowRect.top = y + shadowDy;
        shadowRect.right = 1920;
        shadowRect.bottom = 1080;
        DrawTextW(hdc, (wchar_t*)text, -1, &shadowRect, format);
    } else {
        TextOutW(hdc, x + shadowDx, y + shadowDy, text, (int)wcslen(text));
    }

    SetTextColor(hdc, color);
    if (format != 0) {
        textRect.left = x;
        textRect.top = y;
        textRect.right = 1920;
        textRect.bottom = 1080;
        DrawTextW(hdc, (wchar_t*)text, -1, &textRect, format);
    } else {
        TextOutW(hdc, x, y, text, (int)wcslen(text));
    }

    RestoreDC(hdc, savedDc);
}

static void ui_draw_fallback_shell(AppState *app, HDC hdc, const RECT *clientRect)
{
    HBRUSH bg = CreateSolidBrush(RGB(0xA0, 0xA0, 0xA0));
    HBRUSH dark = CreateSolidBrush(RGB(0x7A, 0x7A, 0x7A));
    RECT r;
    FillRect(hdc, clientRect, bg);
    r.left = 1363; r.top = 134; r.right = 1918; r.bottom = 459; FillRect(hdc, &r, dark);
    r.left = 1363; r.top = 467; r.right = 1918; r.bottom = 618; FillRect(hdc, &r, dark);
    r.left = 1363; r.top = 658; r.right = 1918; r.bottom = 768; FillRect(hdc, &r, dark);
    r.left = 1363; r.top = 958; r.right = 1918; r.bottom = 1047; FillRect(hdc, &r, dark);
    ui_draw_shadowed_text(hdc, app->fonts.title, L"HYPERPLAYER", 1375, 12, COLOR_WHITE, COLOR_SHADOW, 3, 3, NULL, 0);
    DeleteObject(dark);
    DeleteObject(bg);
}

bool ui_load_assets(AppState *app, void *renderer)
{
    wchar_t cursorPath[MAX_PATH];
    app->fonts.pattern = ui_make_font(FONT_FACE, 18, FW_NORMAL);
    app->fonts.sampleList = ui_make_font(FONT_FACE, 17, FW_NORMAL);
    app->fonts.info = ui_make_font(FONT_FACE, 25, FW_NORMAL);
    app->fonts.info2 = ui_make_font(FONT_FACE, 24, FW_NORMAL);
    app->fonts.dir = ui_make_font(FONT_FACE, 16, FW_NORMAL);
    app->fonts.driveButtons = ui_make_font(FONT_FACE, 16, FW_NORMAL);
    app->fonts.waveform = ui_make_font(FONT_FACE, 32, FW_NORMAL);
    app->fonts.title = ui_make_font(FONT_FACE, 42, FW_BOLD);
    
    ui_load_image_portable(renderer, app->backgroundPath, &app->background);
    app->backgroundLoaded = (app->background.pixels != NULL);

    app_join_path(cursorPath, MAX_PATH, app->exeDir, L"PT-mousecursor.png");
    mousecursor_load(cursorPath, 0, 0);

    return true;
}

void ui_release_assets(AppState *app)
{
    ui_delete_font(&app->fonts.pattern);
    ui_delete_font(&app->fonts.sampleList);
    ui_delete_font(&app->fonts.info);
    ui_delete_font(&app->fonts.info2);
    ui_delete_font(&app->fonts.dir);
    ui_delete_font(&app->fonts.driveButtons);
    ui_delete_font(&app->fonts.waveform);
    ui_delete_font(&app->fonts.title);
    ui_free_image(&app->background);
}

void ui_draw(AppState *app, HDC hdc, const RECT *clientRect)
{
    if (!app || !hdc || !clientRect) return;
    
    if (app->backgroundLoaded) {
        ui_draw_image(hdc, clientRect, &app->background);
    } else {
        ui_draw_fallback_shell(app, hdc, clientRect);
    }

    if (app->showFileBrowser) {
        directory_listing_draw(app, hdc);
    } else {
        tunnelvisualizer_draw(app, hdc);
    }

    pattern_view_draw(app, hdc);
    spectrumanalyzer_draw(app, hdc);
    vumeter_draw(app, hdc);
    quadrascope_draw(app, hdc);
    sample_display_draw(app, hdc);
    sample_list_usage_trigger_draw(app, hdc);
    sample_list_draw(app, hdc);
    player_draw_songinfo(app, hdc);
}
