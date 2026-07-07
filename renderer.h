#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>
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

typedef struct HP_Font_Opaque* HP_Font;

typedef struct {
    void *renderer;
    HP_Font currentFont;
    HP_Color textColor;
    HP_Color drawColor;
    HP_Point currentPos;
    void *currentTarget;
} HP_DrawContext;

typedef enum {
    HP_BLEND_NONE = 0,
    HP_BLEND_ALPHA,
    HP_BLEND_ADDITIVE
} HP_BlendMode;

// Context management
void hp_renderer_init_context(HP_DrawContext *ctx, void *renderer);
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
void hp_draw_points(HP_DrawContext *ctx, const HP_Point *points, int count);
void hp_draw_fill_rects(HP_DrawContext *ctx, const HP_Rect *rects, int count);

// Text functions
HP_Font hp_load_font(const wchar_t *path, int pixelHeight);
void hp_free_font(HP_Font font);
void hp_draw_set_font(HP_DrawContext *ctx, HP_Font font);
void hp_draw_set_text_color(HP_DrawContext *ctx, HP_Color color);
void hp_draw_text(HP_DrawContext *ctx, int x, int y, const wchar_t *text);
void hp_get_text_size(HP_DrawContext *ctx, const wchar_t *text, int *w, int *h);

// Texture/Surface operations (replacement for AlphaBlend/BitBlt)
typedef struct {
    void *texture;
    int width;
    int height;
} HP_Texture;

void hp_draw_texture(HP_DrawContext *ctx, HP_Texture *tex, const HP_Rect *src, const HP_Rect *dst, uint8_t alpha);
bool hp_create_streaming_texture(HP_DrawContext *ctx, HP_Texture *outTex, int width, int height);
void hp_destroy_texture(HP_Texture *tex);
void hp_update_texture(HP_Texture *tex, const void *pixels, int pitch);
void hp_set_texture_blend_mode(HP_Texture *tex, HP_BlendMode mode);
bool hp_load_texture_file(HP_DrawContext *ctx, const wchar_t *path, HP_Texture *outTex);

#endif // RENDERER_H
