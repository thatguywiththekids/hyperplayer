#include "renderer.h"
#include "portability.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void hp_renderer_init_context(HP_DrawContext *ctx, void *renderer) {
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

void hp_draw_points(HP_DrawContext *ctx, const HP_Point *points, int count) {
    if (count <= 0) return;
    SDL_FPoint *fpoints = malloc(sizeof(SDL_FPoint) * count);
    if (!fpoints) return;
    for (int i = 0; i < count; ++i) {
        fpoints[i].x = (float)points[i].x;
        fpoints[i].y = (float)points[i].y;
    }
    SDL_RenderPoints(ctx->renderer, fpoints, count);
    free(fpoints);
}

void hp_draw_fill_rects(HP_DrawContext *ctx, const HP_Rect *rects, int count) {
    if (count <= 0) return;
    SDL_FRect *frects = malloc(sizeof(SDL_FRect) * count);
    if (!frects) return;
    for (int i = 0; i < count; ++i) {
        frects[i].x = (float)rects[i].x;
        frects[i].y = (float)rects[i].y;
        frects[i].w = (float)rects[i].w;
        frects[i].h = (float)rects[i].h;
    }
    SDL_RenderFillRects(ctx->renderer, frects, count);
    free(frects);
}

HP_Font hp_load_font(const wchar_t *path, int pixelHeight) {
    char utf8Path[4096];
    hp_wstr_to_utf8(utf8Path, sizeof(utf8Path), path);
    TTF_Font *font = TTF_OpenFont(utf8Path, (float)pixelHeight);
    if (!font) {
        font = TTF_OpenFont("protracker.ttf", (float)pixelHeight);
    }
    if (!font) {
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", (float)pixelHeight);
    }
    return (HP_Font)font;
}

static wchar_t *hp_wcsdup(const wchar_t *src) {
    if (!src) return NULL;
    size_t len = wcslen(src);
    wchar_t *dst = malloc((len + 1) * sizeof(wchar_t));
    if (dst) {
        memcpy(dst, src, (len + 1) * sizeof(wchar_t));
    }
    return dst;
}

#define TEXT_CACHE_SIZE 1024

typedef struct {
    HP_Font font;
    wchar_t *text;
    HP_Color color;
    SDL_Texture *texture;
    int width;
    int height;
    uint64_t last_used;
} TextCacheEntry;

static TextCacheEntry g_text_cache[TEXT_CACHE_SIZE] = {0};

/*
 * Hash function for the text cache key (font pointer, color, and string contents).
 * Uses a modified DJB2 algorithm to mix key values.
 */
static uint32_t hash_text_key(HP_Font font, const wchar_t *text, HP_Color color) {
    uint32_t hash = 5381;
    uintptr_t fontVal = (uintptr_t)font;
    hash = ((hash << 5) + hash) + (uint32_t)(fontVal & 0xFFFFFFFF);
#if UINTPTR_MAX > 0xFFFFFFFF
    hash = ((hash << 5) + hash) + (uint32_t)((fontVal >> 32) & 0xFFFFFFFF);
#endif
    hash = ((hash << 5) + hash) + color.r;
    hash = ((hash << 5) + hash) + color.g;
    hash = ((hash << 5) + hash) + color.b;
    hash = ((hash << 5) + hash) + color.a;
    while (*text) {
        hash = ((hash << 5) + hash) + *text++;
    }
    return hash;
}

/*
 * Looks up a text entry in the texture cache.
 * Implements a 16-slot linear probe to handle hash collisions.
 * If all 16 slots are full and no match is found, the Least Recently Used (LRU)
 * entry in the neighborhood is evicted and replaced to bound VRAM usage.
 */
static TextCacheEntry *find_or_create_cache_entry(HP_DrawContext *ctx, HP_Font font, const wchar_t *text, HP_Color color) {
    uint32_t hash = hash_text_key(font, text, color);
    int best_slot = -1;
    uint64_t oldest_time = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t now = SDL_GetTicks();
    
    for (int i = 0; i < 16; ++i) {
        int idx = (hash + i) & (TEXT_CACHE_SIZE - 1);
        TextCacheEntry *entry = &g_text_cache[idx];
        if (entry->text == NULL) {
            best_slot = idx;
            break;
        }
        if (entry->font == font && 
            entry->color.r == color.r && entry->color.g == color.g &&
            entry->color.b == color.b && entry->color.a == color.a &&
            wcscmp(entry->text, text) == 0) {
            entry->last_used = now;
            return entry;
        }
        if (entry->last_used < oldest_time) {
            oldest_time = entry->last_used;
            best_slot = idx;
        }
    }
    
    TextCacheEntry *entry = &g_text_cache[best_slot];
    if (entry->text) {
        free(entry->text);
        if (entry->texture) {
            SDL_DestroyTexture(entry->texture);
        }
        memset(entry, 0, sizeof(TextCacheEntry));
    }
    
    char mbs[4096];
    hp_wstr_to_utf8(mbs, sizeof(mbs), text);
    SDL_Color sdlColor = {color.r, color.g, color.b, color.a};
    SDL_Surface *surface = TTF_RenderText_Blended((TTF_Font*)font, mbs, 0, sdlColor);
    if (!surface) return NULL;
    
    SDL_Texture *texture = SDL_CreateTextureFromSurface((SDL_Renderer*)ctx->renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return NULL;
    }
    
    entry->font = font;
    entry->text = hp_wcsdup(text);
    entry->color = color;
    entry->texture = texture;
    entry->width = (int)surface->w;
    entry->height = (int)surface->h;
    entry->last_used = now;
    
    SDL_DestroySurface(surface);
    return entry;
}

void hp_free_font(HP_Font font) {
    if (font) {
        for (int i = 0; i < TEXT_CACHE_SIZE; ++i) {
            if (g_text_cache[i].font == font) {
                free(g_text_cache[i].text);
                if (g_text_cache[i].texture) {
                    SDL_DestroyTexture(g_text_cache[i].texture);
                }
                memset(&g_text_cache[i], 0, sizeof(TextCacheEntry));
            }
        }
        TTF_CloseFont((TTF_Font*)font);
    }
}

void hp_draw_set_font(HP_DrawContext *ctx, HP_Font font) {
    ctx->currentFont = font;
}

void hp_draw_set_text_color(HP_DrawContext *ctx, HP_Color color) {
    ctx->textColor = color;
}

void hp_draw_text(HP_DrawContext *ctx, int x, int y, const wchar_t *text) {
    if (!ctx->currentFont || !text) return;
    TextCacheEntry *entry = find_or_create_cache_entry(ctx, ctx->currentFont, text, ctx->textColor);
    if (entry && entry->texture) {
        SDL_FRect dst = {(float)x, (float)y, (float)entry->width, (float)entry->height};
        SDL_RenderTexture((SDL_Renderer*)ctx->renderer, entry->texture, NULL, &dst);
    }
}

void hp_get_text_size(HP_DrawContext *ctx, const wchar_t *text, int *w, int *h) {
    if (!ctx->currentFont || !text) { if (w) *w = 0; if (h) *h = 0; return; }
    char mbs[4096];
    hp_wstr_to_utf8(mbs, sizeof(mbs), text);
    TTF_GetStringSize((TTF_Font*)ctx->currentFont, mbs, 0, w, h);
}

void hp_draw_texture(HP_DrawContext *ctx, HP_Texture *tex, const HP_Rect *src, const HP_Rect *dst, uint8_t alpha) {
    if (!tex || !tex->texture) return;
    SDL_FRect s = src ? (SDL_FRect){(float)src->x, (float)src->y, (float)src->w, (float)src->h} : (SDL_FRect){0, 0, (float)tex->width, (float)tex->height};
    SDL_FRect d = dst ? (SDL_FRect){(float)dst->x, (float)dst->y, (float)dst->w, (float)dst->h} : (SDL_FRect){0, 0, (float)tex->width, (float)tex->height};
    SDL_SetTextureAlphaMod(tex->texture, alpha);
    SDL_RenderTexture(ctx->renderer, tex->texture, &s, &d);
}

bool hp_create_streaming_texture(HP_DrawContext *ctx, HP_Texture *outTex, int width, int height) {
    if (!ctx || !outTex) return false;
    SDL_Texture *tex = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!tex) {
        outTex->texture = NULL;
        outTex->width = 0;
        outTex->height = 0;
        return false;
    }
    outTex->texture = tex;
    outTex->width = width;
    outTex->height = height;
    return true;
}

void hp_destroy_texture(HP_Texture *tex) {
    if (tex && tex->texture) {
        SDL_DestroyTexture((SDL_Texture*)tex->texture);
        tex->texture = NULL;
        tex->width = 0;
        tex->height = 0;
    }
}

void hp_update_texture(HP_Texture *tex, const void *pixels, int pitch) {
    if (tex && tex->texture) {
        SDL_UpdateTexture((SDL_Texture*)tex->texture, NULL, pixels, pitch);
    }
}

void hp_set_texture_blend_mode(HP_Texture *tex, HP_BlendMode mode) {
    if (tex && tex->texture) {
        SDL_BlendMode sdlMode;
        switch (mode) {
            case HP_BLEND_ADDITIVE: sdlMode = SDL_BLENDMODE_ADD; break;
            case HP_BLEND_NONE:     sdlMode = SDL_BLENDMODE_NONE; break;
            default:                sdlMode = SDL_BLENDMODE_BLEND; break;
        }
        SDL_SetTextureBlendMode((SDL_Texture*)tex->texture, sdlMode);
    }
}

bool hp_load_texture_file(HP_DrawContext *ctx, const wchar_t *path, HP_Texture *outTex) {
    if (!ctx || !path || !outTex) return false;
    char utf8Path[4096];
    hp_wstr_to_utf8(utf8Path, sizeof(utf8Path), path);
    SDL_Surface *surface = IMG_Load(utf8Path);
    if (!surface) {
        outTex->texture = NULL;
        outTex->width = 0;
        outTex->height = 0;
        return false;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface((SDL_Renderer*)ctx->renderer, surface);
    outTex->width = surface->w;
    outTex->height = surface->h;
    SDL_DestroySurface(surface);
    if (!tex) {
        outTex->texture = NULL;
        outTex->width = 0;
        outTex->height = 0;
        return false;
    }
    outTex->texture = tex;
    return true;
}
