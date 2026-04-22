#include "quadrascope.h"
#include "player.h"
#include "renderer.h"
#include "portability.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct ScopeBox {
    int x;
    int y;
    int w;
    int h;
} ScopeBox;

static const ScopeBox g_boxes[4] = {
    {1382, 958, 113, 89},
    {1517, 958, 113, 89},
    {1652, 958, 113, 89},
    {1787, 958, 113, 89}
};

static HP_Color quadrascope_get_waveform_color(AppState *app)
{
    static bool loaded = false;
    static HP_Color color = {255, 221, 0, 255};
    if (!loaded && app) {
        color = app_ini_get_color(app, L"QUADRASCOPE", L"QUADRACOLOR", (HP_Color){255, 221, 0, 255});
        loaded = true;
    }
    return color;
}

static float clampf_local(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool sample_has_loop(int sampleCount, int loopStart, int loopLength)
{
    int loopStart1 = loopStart + 1;
    return (loopLength > 2 && loopStart1 <= sampleCount);
}

static float get_sample_value(const float *values, int sampleCount, double pos, bool useLoopMode, int loopStart, int loopLength)
{
    int loopStart1 = loopStart + 1;
    if (!values || sampleCount < 1) return 0.0f;

    if (useLoopMode && loopLength > 2 && loopStart1 <= sampleCount) {
        int loopEnd = loopStart1 + loopLength - 1;
        if (loopEnd > sampleCount) loopEnd = sampleCount;
        if (pos < 1.0) pos = 1.0;
        if (pos > (double)loopEnd) {
            int loopSpan = loopEnd - loopStart1 + 1;
            if (loopSpan < 1) loopSpan = 1;
            pos = loopStart1 + fmod((pos - loopStart1), (double)loopSpan);
        }
    } else {
        if (pos < 1.0) pos = 1.0;
        else if (pos > (double)sampleCount) pos = (double)sampleCount;
    }

    int index = (int)floor(pos);
    if (index < 1) index = 1;
    else if (index > sampleCount) index = sampleCount;
    return values[index - 1];
}

static double compute_window_start(int sampleCount, double currentPos, int width, int stride, bool useLoopMode)
{
    double startPos = currentPos - floor((double)(width * stride) * 0.5);
    if (useLoopMode) return startPos;

    int visibleSpan = (width - 1) * stride;
    int maxStart = sampleCount - visibleSpan;
    if (maxStart < 1) maxStart = 1;

    if (startPos < 1.0) startPos = 1.0;
    else if (startPos > (double)maxStart) startPos = (double)maxStart;
    return startPos;
}

static void draw_scope_from_state(
    AppState *app,
    HP_DrawContext *ctx,
    const float *values,
    int sampleCount,
    int loopStart,
    int loopLength,
    const QuadrascopeState *state,
    const ScopeBox *box
)
{
    if (!ctx || !values || sampleCount < 2 || !state || !box) return;
    if (state->sampleIndex <= 0) return;
    if ((!state->active) && state->vu <= 0.0f && state->scopeHold <= 0.0) return;

    bool useLoopMode = sample_has_loop(sampleCount, loopStart, loopLength);
    int stride = state->scopeStride < 1 ? 1 : state->scopeStride;

    double startPos = compute_window_start(sampleCount, state->samplePos, box->w, stride, useLoopMode);
    float ampScale = clampf_local((state->sampleVolume * 0.65f) + (state->vu * 0.85f), 0.12f, 1.0f);
    float amp = (float)(box->h * 0.44f) * ampScale;
    double midY = box->y + (box->h * 0.5);

    HP_Point points[113];
    float peak = 0.0f;
    for (int i = 0; i < box->w; ++i) {
        double pos = startPos + (double)(i * stride);
        float v = get_sample_value(values, sampleCount, pos, useLoopMode, loopStart, loopLength);
        float a = fabsf(v);
        if (a > peak) peak = a;

        points[i].x = box->x + i;
        points[i].y = (int)lround(midY - (v * amp));
    }

    if (peak <= 0.0001f) return;

    hp_draw_set_color(ctx, quadrascope_get_waveform_color(app));
    hp_draw_polyline(ctx, points, box->w);
}

void quadrascope_draw(AppState *app, HP_DrawContext *ctx)
{
    if (!app || !ctx || !player_is_loaded(app)) return;

    for (int channel = 1; channel <= 4; ++channel) {
        QuadrascopeState state;
        const float *values = NULL;
        int sampleCount = 0;
        int loopStart = 0;
        int loopLength = 0;

        if (!player_get_quadrascope_state(app, channel, &state)) continue;
        if (!player_get_sample_values(app, state.sampleIndex, &values, &sampleCount, &loopStart, &loopLength)) continue;

        draw_scope_from_state(app, ctx, values, sampleCount, loopStart, loopLength, &state, &g_boxes[channel - 1]);
    }
}
