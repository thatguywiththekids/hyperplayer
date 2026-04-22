#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <wchar.h>

// Standard types
typedef struct {
    uint8_t r, g, b, a;
} HP_Color;

typedef struct {
    int x, y, w, h;
} HP_Rect;

typedef struct {
    int x, y;
} HP_Point;

typedef TTF_Font* HP_Font;

typedef struct {
    SDL_Renderer *renderer;
    HP_Font currentFont;
    HP_Color textColor;
    HP_Color drawColor;
    HP_Point currentPos;
    SDL_Texture *currentTarget;
} HP_DrawContext;

typedef enum {
    HP_BLEND_NONE = 0,
    HP_BLEND_ALPHA,
    HP_BLEND_ADDITIVE
} HP_BlendMode;

// Context management
void hp_renderer_init_context(HP_DrawContext *ctx, SDL_Renderer *renderer);
void hp_draw_set_blend_mode(HP_DrawContext *ctx, HP_BlendMode mode);

// Color utilities
HP_Color hp_color_rgb(uint8_t r, uint8_t g, uint8_t b);
HP_Color hp_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Drawing primitives
void hp_draw_set_color(HP_DrawContext *ctx, HP_Color color);
void hp_draw_fill_rect(HP_DrawContext *ctx, const HP_Rect *rect);
void hp_draw_rect(HP_DrawContext *ctx, const HP_Rect *rect);
void hp_draw_line(HP_DrawContext *ctx, int x1, int y1, int x2, int y2);
void hp_draw_polyline(HP_DrawContext *ctx, const HP_Point *points, int count);
void hp_draw_pixel(HP_DrawContext *ctx, int x, int y, HP_Color color);

// Text functions
void hp_draw_set_font(HP_DrawContext *ctx, HP_Font font);
void hp_draw_set_text_color(HP_DrawContext *ctx, HP_Color color);
void hp_draw_text(HP_DrawContext *ctx, int x, int y, const wchar_t *text);
void hp_get_text_size(HP_DrawContext *ctx, const wchar_t *text, int *w, int *h);

// Texture/Surface operations (replacement for AlphaBlend/BitBlt)
typedef struct {
    SDL_Texture *texture;
    int width;
    int height;
} HP_Texture;

void hp_draw_texture(HP_DrawContext *ctx, HP_Texture *tex, const HP_Rect *src, const HP_Rect *dst, uint8_t alpha);

#endif // RENDERER_H
