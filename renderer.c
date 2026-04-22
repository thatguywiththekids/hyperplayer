#include "renderer.h"
#include "portability.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#ifndef _WIN32
#include <time.h>

// Forward declarations for shims defined here
DWORD GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName, const wchar_t* lpDefault, wchar_t* lpReturnedString, DWORD nSize, const wchar_t* lpFileName);

uint64_t GetTickCount64(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

DWORD GetModuleFileNameW(HANDLE hModule, wchar_t* lpFilename, DWORD nSize) {
    (void)hModule;
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        mbstowcs(lpFilename, path, nSize);
        return (DWORD)wcslen(lpFilename);
    }
    return 0;
}

static void trim_wstr(wchar_t* s) {
    wchar_t* p = s;
    while (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n') p++;
    if (p != s) memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));
    int len = (int)wcslen(s);
    while (len > 0 && (s[len - 1] == L' ' || s[len - 1] == L'\t' || s[len - 1] == L'\r' || s[len - 1] == L'\n')) {
        s[--len] = L'\0';
    }
}

DWORD GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName, const wchar_t* lpDefault, wchar_t* lpReturnedString, DWORD nSize, const wchar_t* lpFileName) {
    char mbsFile[1024];
    wcstombs(mbsFile, lpFileName, sizeof(mbsFile));
    FILE* f = fopen(mbsFile, "r");
    if (!f) {
        if (lpDefault) wcsncpy(lpReturnedString, lpDefault, nSize);
        else lpReturnedString[0] = L'\0';
        return (DWORD)wcslen(lpReturnedString);
    }

    wchar_t line[1024];
    wchar_t currentSection[256] = L"";
    bool found = false;

    while (fgetws(line, 1024, f)) {
        trim_wstr(line);
        if (line[0] == L'[' ) {
            wchar_t* end = wcsrchr(line, L']');
            if (end) {
                *end = L'\0';
                wcsncpy(currentSection, line + 1, 255);
            }
        } else if (wcscmp(currentSection, lpAppName) == 0) {
            wchar_t* sep = wcschr(line, L'=');
            if (sep) {
                *sep = L'\0';
                wchar_t* key = line;
                wchar_t* val = sep + 1;
                trim_wstr(key);
                trim_wstr(val);
                if (wcscmp(key, lpKeyName) == 0) {
                    wcsncpy(lpReturnedString, val, nSize);
                    found = true;
                    break;
                }
            }
        }
    }
    fclose(f);

    if (!found) {
        if (lpDefault) wcsncpy(lpReturnedString, lpDefault, nSize);
        else lpReturnedString[0] = L'\0';
    }
    return (DWORD)wcslen(lpReturnedString);
}

UINT GetPrivateProfileIntW(const wchar_t* lpAppName, const wchar_t* lpKeyName, int nDefault, const wchar_t* lpFileName) {
    wchar_t buf[64];
    if (GetPrivateProfileStringW(lpAppName, lpKeyName, NULL, buf, 64, lpFileName) > 0) {
        return (UINT)wcstol(buf, NULL, 10);
    }
    return (UINT)nDefault;
}

void* ShellExecuteW(HWND hwnd, const wchar_t* lpOperation, const wchar_t* lpFile, const wchar_t* lpParameters, const wchar_t* lpDirectory, int nShowCmd) {
    (void)hwnd; (void)lpOperation; (void)lpParameters; (void)lpDirectory; (void)nShowCmd;
    if (!lpFile) return (void*)0;
    
    char url[4096];
    size_t converted = wcstombs(url, lpFile, sizeof(url) - 1);
    if (converted != (size_t)-1) {
        url[converted] = '\0';
        SDL_OpenURL(url);
        return (void*)33; // Value > 32 indicates success in Win32
    }
    return (void*)0;
}
#endif

void hp_renderer_init_context(HP_DrawContext *ctx, SDL_Renderer *renderer) {
    memset(ctx, 0, sizeof(HP_DrawContext));
    ctx->renderer = renderer;
    ctx->textColor = (HP_Color){255, 255, 255, 255};
    ctx->drawColor = (HP_Color){255, 255, 255, 255};
}

void hp_draw_set_blend_mode(HP_DrawContext *ctx, HP_BlendMode mode) {
    SDL_BlendMode sdlMode;
    switch (mode) {
        case HP_BLEND_ADDITIVE: sdlMode = SDL_BLENDMODE_ADD; break;
        case HP_BLEND_NONE:     sdlMode = SDL_BLENDMODE_NONE; break;
        default:                sdlMode = SDL_BLENDMODE_BLEND; break;
    }
    SDL_SetRenderDrawBlendMode(ctx->renderer, sdlMode);
}

HP_Color hp_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (HP_Color){r, g, b, 255};
}

HP_Color hp_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (HP_Color){r, g, b, a};
}

void hp_draw_set_color(HP_DrawContext *ctx, HP_Color color) {
    ctx->drawColor = color;
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
}

void hp_draw_fill_rect(HP_DrawContext *ctx, const HP_Rect *rect) {
    SDL_FRect r = {(float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h};
    SDL_RenderFillRect(ctx->renderer, &r);
}

void hp_draw_rect(HP_DrawContext *ctx, const HP_Rect *rect) {
    SDL_FRect r = {(float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h};
    SDL_RenderRect(ctx->renderer, &r);
}

void hp_draw_line(HP_DrawContext *ctx, int x1, int y1, int x2, int y2) {
    SDL_RenderLine(ctx->renderer, (float)x1, (float)y1, (float)x2, (float)y2);
}

void hp_draw_polyline(HP_DrawContext *ctx, const HP_Point *points, int count) {
    if (count < 2) return;
    SDL_FPoint *fpoints = malloc(sizeof(SDL_FPoint) * count);
    for (int i = 0; i < count; ++i) {
        fpoints[i].x = (float)points[i].x;
        fpoints[i].y = (float)points[i].y;
    }
    SDL_RenderLines(ctx->renderer, fpoints, count);
    free(fpoints);
}

void hp_draw_pixel(HP_DrawContext *ctx, int x, int y, HP_Color color) {
    uint8_t or, og, ob, oa;
    SDL_GetRenderDrawColor(ctx->renderer, &or, &og, &ob, &oa);
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoint(ctx->renderer, (float)x, (float)y);
    SDL_SetRenderDrawColor(ctx->renderer, or, og, ob, oa);
}

void hp_draw_set_font(HP_DrawContext *ctx, HP_Font font) {
    ctx->currentFont = font;
}

void hp_draw_set_text_color(HP_DrawContext *ctx, HP_Color color) {
    ctx->textColor = color;
}

void hp_draw_text(HP_DrawContext *ctx, int x, int y, const wchar_t *text) {
    if (!ctx->currentFont || !text) return;
    char mbs[4096];
    wcstombs(mbs, text, sizeof(mbs));
    mbs[4095] = '\0';
    SDL_Color color = {ctx->textColor.r, ctx->textColor.g, ctx->textColor.b, ctx->textColor.a};
    SDL_Surface *surface = TTF_RenderText_Blended(ctx->currentFont, mbs, 0, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);
    if (texture) {
        SDL_FRect dst = {(float)x, (float)y, (float)surface->w, (float)surface->h};
        SDL_RenderTexture(ctx->renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

void hp_get_text_size(HP_DrawContext *ctx, const wchar_t *text, int *w, int *h) {
    if (!ctx->currentFont || !text) { if (w) *w = 0; if (h) *h = 0; return; }
    char mbs[4096];
    wcstombs(mbs, text, sizeof(mbs));
    mbs[4095] = '\0';
    TTF_GetStringSize(ctx->currentFont, mbs, 0, w, h);
}

void hp_draw_texture(HP_DrawContext *ctx, HP_Texture *tex, const HP_Rect *src, const HP_Rect *dst, uint8_t alpha) {
    if (!tex || !tex->texture) return;
    SDL_FRect s = src ? (SDL_FRect){(float)src->x, (float)src->y, (float)src->w, (float)src->h} : (SDL_FRect){0, 0, (float)tex->width, (float)tex->height};
    SDL_FRect d = dst ? (SDL_FRect){(float)dst->x, (float)dst->y, (float)dst->w, (float)dst->h} : (SDL_FRect){0, 0, (float)tex->width, (float)tex->height};
    SDL_SetTextureAlphaMod(tex->texture, alpha);
    SDL_RenderTexture(ctx->renderer, tex->texture, &s, &d);
}
