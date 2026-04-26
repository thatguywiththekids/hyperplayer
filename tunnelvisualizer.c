#include "tunnelvisualizer.h"
#include "player.h"
#include "renderer.h"
#include "portability.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TV_BOX_X 1363
#define TV_BOX_Y 132
#define TV_BOX_W 555
#define TV_BOX_H 329

#define TV_BROWSER_BUTTON_X1 1363
#define TV_BROWSER_BUTTON_X2 1576
#define TV_BROWSER_BUTTON_Y1 102
#define TV_BROWSER_BUTTON_Y2 124

#define FFT_SIZE 1024
#define BAND_COUNT 48
#define MIN_HZ 50.0
#define MAX_HZ 9000.0

typedef struct VisualizerConfig {
    float rotationSpeed;
    float barDeclineSpeed;
    float barLengthScale;
    float colorCycleSpeed;
    float globalGlowStrength;
    float highHzResponse;
    float highHzLengthBonus;
    int starfieldCount;
    float starfieldSpeed;
    float starfieldBrightness;
    int bassOnlyBandCount;
    float bassOnlyDominance;
    float bassOnlyGateScale;
    float bassOnlyAbsLevel;
    int agcIgnoreLowBands;
    float highPresenceFloor;
    HP_Color backgroundColor;
    HP_Color starColor;
} VisualizerConfig;

static const VisualizerConfig g_visualizerDefaults = {
    2.00f, 3.60f, 1.50f, 2.00f, 0.90f, 2.00f, 0.63f, 7000, 0.36f, 1.00f,
    20, 1.10f, 0.85f, 0.018f, 20, 0.90f,
    {6, 6, 10, 255}, {255, 255, 255, 255}
};

static VisualizerConfig g_visualizer;
static bool g_visualizerLoaded = false;

typedef struct RadialState {
    float mono[FFT_SIZE];
    float bands[BAND_COUNT];
    float energy, bass, treble, beatFlash, agcGain, bassGatePrev;
    double time, rotation;
    bool initialized;
} RadialState;

typedef struct BloomSurface {
    SDL_Texture *texture;
    unsigned int *pixels;
    unsigned int *src;
    unsigned int *tmp;
    int width, height;
} BloomSurface;

typedef struct StarfieldState {
    float *x, *y, *z, *speed, *brightness, *size;
    int count, width, height;
    float fov;
    bool ready;
} StarfieldState;

static RadialState g_radial;
static BloomSurface g_bloom = { 0 };
static StarfieldState g_starfield = { 0 };
static unsigned int g_star_rng = 0xA341316Cu;

static double g_window[FFT_SIZE], g_real[FFT_SIZE], g_imag[FFT_SIZE], g_mags[FFT_SIZE / 2];
static int g_bandEdges[BAND_COUNT + 1];
static bool g_fftInit = false;
static int g_sampleRate = 44100;

static float star_rand01(void) { g_star_rng = (g_star_rng * 1664525u) + 1013904223u; return (float)(g_star_rng & 0x00FFFFFFu) / 16777215.0f; }
static float star_rand_range(float a, float b) { return a + ((b - a) * star_rand01()); }

static void visualizer_load_config(AppState *app) {
    if (g_visualizerLoaded) return;
    g_visualizer = g_visualizerDefaults;
    g_visualizer.rotationSpeed = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_ROTATION_SPEED", 2.0);
    g_visualizer.barDeclineSpeed = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_BAR_DECLINE_SPEED", 3.6);
    g_visualizer.barLengthScale = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_BAR_LENGTH_SCALE", 1.5);
    g_visualizer.colorCycleSpeed = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_COLOR_CYCLE_SPEED", 2.0);
    g_visualizer.globalGlowStrength = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_GLOBAL_GLOW_STRENGTH", 0.9);
    g_visualizer.highHzResponse = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_HIGH_HZ_RESPONSE", 2.0);
    g_visualizer.highHzLengthBonus = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_HIGH_HZ_LENGTH_BONUS", 0.63);
    g_visualizer.starfieldCount = app_ini_get_int(app, L"VISUALIZER", L"VIS_STARFIELD_COUNT", 7000);
    g_visualizer.starfieldSpeed = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_STARFIELD_SPEED", 0.36);
    g_visualizer.starfieldBrightness = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_STARFIELD_BRIGHTNESS", 1.0);
    g_visualizer.bassOnlyBandCount = app_ini_get_int(app, L"VISUALIZER", L"VIS_BASS_ONLY_BAND_COUNT", 20);
    g_visualizer.bassOnlyDominance = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_BASS_ONLY_DOMINANCE", 1.1);
    g_visualizer.bassOnlyGateScale = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_BASS_ONLY_GATE_SCALE", 0.85);
    g_visualizer.bassOnlyAbsLevel = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_BASS_ONLY_ABS_LEVEL", 0.018);
    g_visualizer.agcIgnoreLowBands = app_ini_get_int(app, L"VISUALIZER", L"VIS_AGC_IGNORE_LOW_BANDS", 20);
    g_visualizer.highPresenceFloor = (float)app_ini_get_double(app, L"VISUALIZER", L"VIS_HIGH_PRESENCE_FLOOR", 0.9);
    g_visualizer.backgroundColor = app_ini_get_color(app, L"VISUALIZER", L"VIS_BACKGROUND_COLOR", (HP_Color){6, 6, 10, 255});
    g_visualizer.starColor = app_ini_get_color(app, L"VISUALIZER", L"VIS_STAR_COLOR", (HP_Color){255, 255, 255, 255});
    if (g_visualizer.starfieldCount < 1) g_visualizer.starfieldCount = 1;
    g_visualizerLoaded = true;
}

static float clampf_local(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf_local(float a, float b, float t) { return a + ((b - a) * t); }

static void fft_inplace(double *re, double *im, int n) {
    int bits = 0; for (int temp = n; temp > 1; temp >>= 1) bits++;
    for (int i = 0; i < n; ++i) {
        int j = 0, x = i; for (int k = 0; k < bits; ++k) { j = (j << 1) | (x & 1); x >>= 1; }
        if (j > i) { double tr = re[i], ti = im[i]; re[i] = re[j]; im[i] = im[j]; re[j] = tr; im[j] = ti; }
    }
    for (int len = 2; len <= n; len *= 2) {
        double ang = -2.0 * M_PI / (double)len;
        double wlenCos = cos(ang), wlenSin = sin(ang);
        for (int i = 0; i < n; i += len) {
            double wCos = 1.0, wSin = 0.0;
            for (int j = 0; j < len/2; ++j) {
                double uR = re[i+j], uI = im[i+j];
                double vR = re[i+j+len/2]*wCos - im[i+j+len/2]*wSin, vI = re[i+j+len/2]*wSin + im[i+j+len/2]*wCos;
                re[i+j] = uR + vR; im[i+j] = uI + vI; re[i+j+len/2] = uR - vR; im[i+j+len/2] = uI - vI;
                double nC = wCos*wlenCos - wSin*wlenSin, nS = wCos*wlenSin + wSin*wlenCos;
                wCos = nC; wSin = nS;
            }
        }
    }
}

static void init_fft_tables(void) {
    if (g_fftInit) return;
    for (int n = 0; n < FFT_SIZE; ++n) {
        double t = (double)n / (double)(FFT_SIZE - 1);
        g_window[n] = 0.5 * (1.0 - cos(2.0 * M_PI * t)) * (0.20 + 0.80 * t * t * t);
    }
    for (int i = 0; i <= BAND_COUNT; ++i) {
        double hz = exp(log(MIN_HZ) + (log(MAX_HZ) - log(MIN_HZ)) * (double)i / BAND_COUNT);
        int bin = (int)floor(((hz / (double)g_sampleRate) * (double)FFT_SIZE) + 0.5);
        g_bandEdges[i] = bin < 1 ? 1 : (bin > FFT_SIZE/2 ? FFT_SIZE/2 : bin);
    }
    g_fftInit = true;
}

static HP_Color hsv_to_rgb(float h, float s, float v) {
    h = fmodf(h, 1.0f); if (h < 0) h += 1.0f;
    s = clampf_local(s, 0.0f, 1.0f); v = clampf_local(v, 0.0f, 1.0f);
    if (s <= 0.0f) return (HP_Color){(uint8_t)(v*255),(uint8_t)(v*255),(uint8_t)(v*255),255};
    float hh = h * 6.0f; int i = (int)hh; float f = hh - (float)i;
    float p = v*(1-s), q = v*(1-s*f), t = v*(1-s*(1-f));
    switch (i%6) {
        case 0: return (HP_Color){(uint8_t)(v*255),(uint8_t)(t*255),(uint8_t)(p*255),255};
        case 1: return (HP_Color){(uint8_t)(q*255),(uint8_t)(v*255),(uint8_t)(p*255),255};
        case 2: return (HP_Color){(uint8_t)(p*255),(uint8_t)(v*255),(uint8_t)(t*255),255};
        case 3: return (HP_Color){(uint8_t)(p*255),(uint8_t)(q*255),(uint8_t)(v*255),255};
        case 4: return (HP_Color){(uint8_t)(t*255),(uint8_t)(p*255),(uint8_t)(v*255),255};
        default: return (HP_Color){(uint8_t)(v*255),(uint8_t)(p*255),(uint8_t)(q*255),255};
    }
}

static void bloom_release(void) {
    if (g_bloom.texture) SDL_DestroyTexture(g_bloom.texture);
    if (g_bloom.src) free(g_bloom.src); if (g_bloom.tmp) free(g_bloom.tmp);
    if (g_bloom.pixels) free(g_bloom.pixels);
    memset(&g_bloom, 0, sizeof(g_bloom));
}

static bool bloom_ensure(HP_DrawContext *ctx, int width, int height) {
    if (g_bloom.texture && g_bloom.width == width && g_bloom.height == height) return true;
    bloom_release();
    g_bloom.texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_bloom.texture) return false;
    g_bloom.src = malloc(width * height * 4); g_bloom.tmp = malloc(width * height * 4);
    g_bloom.pixels = malloc(width * height * 4);
    if (!g_bloom.src || !g_bloom.tmp || !g_bloom.pixels) { bloom_release(); return false; }
    g_bloom.width = width; g_bloom.height = height;
    return true;
}

static inline unsigned int pack_bgra(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return (a << 24) | (r << 16) | (g << 8) | b; }

static void buffer_add_soft_ellipse(unsigned int *buffer, int w, int h, float cx, float cy, float rx, float ry, HP_Color color, float strength) {
    int minX = (int)fmaxf(0, cx-rx-1), maxX = (int)fminf(w-1, cx+rx+1), minY = (int)fmaxf(0, cy-ry-1), maxY = (int)fminf(h-1, cy+ry+1);
    float irx2 = 1.0f/(rx*rx), iry2 = 1.0f/(ry*ry);
    for (int y = minY; y <= maxY; ++y) {
        float dy2 = powf(y+0.5f-cy, 2)*iry2;
        for (int x = minX; x <= maxX; ++x) {
            float d2 = powf(x+0.5f-cx, 2)*irx2 + dy2;
            if (d2 < 1.0f) {
                float glow = powf(1.0f - sqrtf(d2), 2) * strength;
                unsigned int p = buffer[y*w+x];
                int r = (int)((p >> 16) & 255) + (int)(color.r * glow);
                int g = (int)((p >> 8) & 255) + (int)(color.g * glow);
                int b = (int)(p & 255) + (int)(color.b * glow);
                buffer[y*w+x] = pack_bgra(r>255?255:r, g>255?255:g, b>255?255:b, 255);
            }
        }
    }
}

static void buffer_add_soft_line(unsigned int *buffer, int w, int h, float x1, float y1, float x2, float y2, float radius, HP_Color color, float strength) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx*dx + dy*dy);
    int steps = (int)(len * 1.25f) + 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps;
        buffer_add_soft_ellipse(buffer, w, h, x1 + dx*t, y1 + dy*t, radius, radius, color, strength);
    }
}

static void blur_horizontal(const unsigned int *src, unsigned int *dst, int w, int h, int radius) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r=0, g=0, b=0, count=0;
            for (int k = -radius; k <= radius; ++k) {
                int xx = x + k; if (xx < 0 || xx >= w) continue;
                unsigned int p = src[y*w + xx];
                b += (p & 255); g += ((p >> 8) & 255); r += ((p >> 16) & 255); count++;
            }
            if (count > 0) dst[y*w + x] = pack_bgra(r/count, g/count, b/count, 255);
            else dst[y*w + x] = src[y*w + x];
        }
    }
}

static void blur_vertical_to_premult(const unsigned int *src, unsigned int *dst, int w, int h, int radius, float strength) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r=0, g=0, b=0, count=0;
            for (int k = -radius; k <= radius; ++k) {
                int yy = y + k; if (yy < 0 || yy >= h) continue;
                unsigned int p = src[yy*w + x];
                b += (p & 255); g += ((p >> 8) & 255); r += ((p >> 16) & 255); count++;
            }
            if (count > 0) {
                int rr = (int)(r/count * strength), gg = (int)(g/count * strength), bb = (int)(b/count * strength);
                if (rr>255) rr=255; if (gg>255) gg=255; if (bb>255) bb=255;
                int aa = rr; if (gg>aa) aa=gg; if (bb>aa) aa=bb;
                dst[y*w + x] = pack_bgra(rr, gg, bb, aa);
            } else {
                dst[y*w + x] = src[y*w + x];
            }
        }
    }
}

static void starfield_release(void) {
    free(g_starfield.x); free(g_starfield.y); free(g_starfield.z);
    free(g_starfield.speed); free(g_starfield.brightness); free(g_starfield.size);
    memset(&g_starfield, 0, sizeof(g_starfield));
}

static void starfield_reset_one(int i) {
    g_starfield.x[i] = star_rand_range(-(float)g_starfield.width, (float)g_starfield.width);
    g_starfield.y[i] = star_rand_range(-(float)g_starfield.height, (float)g_starfield.height);
    g_starfield.z[i] = star_rand_range(1.0f, (float)g_starfield.width);
    g_starfield.speed[i] = star_rand_range(20.0f, 150.0f);
    g_starfield.brightness[i] = star_rand_range(0.25f, 1.0f);
    g_starfield.size[i] = star_rand_range(1.0f, 3.0f);
}

static bool starfield_ensure(int count, int width, int height) {
    if (g_starfield.ready && g_starfield.count == count) return true;
    starfield_release();
    g_starfield.x = malloc(count * sizeof(float)); g_starfield.y = malloc(count * sizeof(float)); g_starfield.z = malloc(count * sizeof(float));
    g_starfield.speed = malloc(count * sizeof(float)); g_starfield.brightness = malloc(count * sizeof(float)); g_starfield.size = malloc(count * sizeof(float));
    if (!g_starfield.x || !g_starfield.y || !g_starfield.z || !g_starfield.speed || !g_starfield.brightness || !g_starfield.size) return false;
    g_starfield.count = count; g_starfield.width = width; g_starfield.height = height; g_starfield.fov = width * 0.5f;
    for (int i=0; i<count; ++i) starfield_reset_one(i);
    return g_starfield.ready = true;
}

void tunnelvisualizer_update(AppState *app, double dt) {
    visualizer_load_config(app); init_fft_tables();
    starfield_ensure(g_visualizer.starfieldCount, TV_BOX_W, TV_BOX_H);
    if (!g_radial.initialized) { memset(&g_radial, 0, sizeof(g_radial)); g_radial.agcGain = 1.0f; g_radial.initialized = true; }
    
    dt = clampf_local((float)dt, 0.0f, 0.1f);
    float mono[FFT_SIZE];
    if (app && player_is_loaded(app) && !player_is_paused(app) && player_get_recent_mono_window(app, mono, FFT_SIZE)) {
        for (int i=0; i<FFT_SIZE; ++i) { g_real[i] = mono[i]*g_window[i]; g_imag[i] = 0.0; }
        fft_inplace(g_real, g_imag, FFT_SIZE);
        for (int k=0; k<FFT_SIZE/2; ++k) g_mags[k] = sqrt(g_real[k]*g_real[k] + g_imag[k]*g_imag[k]);
        
        float meanRaw = 0.0f;
        float frameBass = 0.0f, frameMid = 0.0f, frameHigh = 0.0f;
        for (int i=0; i<BAND_COUNT; ++i) {
            int a = g_bandEdges[i], b = g_bandEdges[i+1]-1;
            if (b < a) b = a;
            int binCount = b - a + 1;
            float sum = 0.0f; for (int k=a; k<=b; ++k) sum += (float)g_mags[k-1];
            float raw = log10f(1.0f + (sum / (float)binCount) * 4.2f);
            if (i >= g_visualizer.agcIgnoreLowBands) meanRaw += raw;
            
            float hiRamp = (float)i/(BAND_COUNT-1);
            float target = raw * (1.0f + powf(hiRamp, 1.8f)*(g_visualizer.highHzResponse-1.0f)) * g_radial.agcGain;
            target = powf(clampf_local(target, 0, 1), 1.1f);
            g_radial.bands[i] = lerpf_local(g_radial.bands[i], target, target > g_radial.bands[i] ? 0.98f : 0.82f);
            g_radial.bands[i] -= dt * g_visualizer.barDeclineSpeed;
            if (g_radial.bands[i] < 0) g_radial.bands[i] = 0;
            
            if (i < 8) frameBass += g_radial.bands[i];
            else if (i < 24) frameMid += g_radial.bands[i];
            else frameHigh += g_radial.bands[i];
        }
        g_radial.bass = frameBass / 8.0f;
        g_radial.treble = frameHigh / (BAND_COUNT - 24);
        
        float frameEnergy = (frameBass + frameMid + frameHigh) / BAND_COUNT;
        if (frameEnergy > g_radial.energy * 1.5f && frameEnergy > 0.15f) g_radial.beatFlash = 1.0f;
        else g_radial.beatFlash = lerpf_local(g_radial.beatFlash, 0, 0.15f);
        
        g_radial.energy = lerpf_local(g_radial.energy, frameEnergy, 0.72f);
        g_radial.rotation += dt * (0.10f + g_radial.energy*0.18f) * g_visualizer.rotationSpeed;

        // Smoother, less aggressive AGC
        float targetGain = clampf_local(0.22f/(meanRaw/(BAND_COUNT-g_visualizer.agcIgnoreLowBands)+0.0001f), 0.35f, 2.8f);
        g_radial.agcGain = lerpf_local(g_radial.agcGain, targetGain, 0.08f); 
    } else {
        for (int i=0; i<BAND_COUNT; ++i) { g_radial.bands[i] = lerpf_local(g_radial.bands[i], 0, 0.7f); g_radial.bands[i] -= dt*g_visualizer.barDeclineSpeed; if (g_radial.bands[i]<0) g_radial.bands[i]=0; }
        g_radial.energy = lerpf_local(g_radial.energy, 0, 0.55f);
        g_radial.bass = lerpf_local(g_radial.bass, 0, 0.55f);
        g_radial.treble = lerpf_local(g_radial.treble, 0, 0.55f);
        g_radial.beatFlash = lerpf_local(g_radial.beatFlash, 0, 0.15f);
        g_radial.rotation += dt * 0.03f * g_visualizer.rotationSpeed;
    }
    for (int i=0; i<g_starfield.count; ++i) {
        g_starfield.z[i] -= g_starfield.speed[i] * g_visualizer.starfieldSpeed * dt;
        if (g_starfield.z[i] < 1.0f) starfield_reset_one(i);
        g_starfield.brightness[i] = clampf_local(g_starfield.brightness[i] + (star_rand01()-0.5f)*0.05f, 0.15f, 1.0f);
    }
}

bool tunnelvisualizer_mouse_down(AppState *app, int x, int y) {
    if (!app || !player_is_loaded(app)) return false;
    if (x >= TV_BROWSER_BUTTON_X1 && x <= TV_BROWSER_BUTTON_X2 && y >= TV_BROWSER_BUTTON_Y1 && y <= TV_BROWSER_BUTTON_Y2) {
        app->showFileBrowser = !app->showFileBrowser; return true;
    }
    return false;
}

void tunnelvisualizer_draw(AppState *app, HP_DrawContext *ctx) {
    if (!app || !ctx || app->showFileBrowser || !player_is_loaded(app)) return;
    visualizer_load_config(app);

    HP_Rect box = {TV_BOX_X, TV_BOX_Y, TV_BOX_W, TV_BOX_H};
    hp_draw_set_color(ctx, g_visualizer.backgroundColor);
    hp_draw_fill_rect(ctx, &box);

    // Batched Starfield
    static SDL_FPoint pts[10000]; static SDL_FRect r2[10000], r3[10000];
    int n1=0, n2=0, n3=0;
    for (int i=0; i<g_starfield.count; ++i) {
        float f = g_starfield.fov / g_starfield.z[i];
        float sx = g_starfield.x[i]*f + TV_BOX_W/2, sy = g_starfield.y[i]*f + TV_BOX_H/2;
        if (sx<0 || sx>=TV_BOX_W || sy<0 || sy>=TV_BOX_H) continue;
        float b = clampf_local((1.0f - g_starfield.z[i]/g_starfield.width)*g_starfield.brightness[i]*g_visualizer.starfieldBrightness, 0, 1);
        if (b < 0.1f) continue;
        float px = TV_BOX_X + sx, py = TV_BOX_Y + sy;
        if (b > 0.78f) { r3[n3++] = (SDL_FRect){px, py, 3, 3}; }
        else if (b > 0.40f) { r2[n2++] = (SDL_FRect){px, py, 2, 2}; }
        else { pts[n1++] = (SDL_FPoint){px, py}; }
    }
    hp_draw_set_color(ctx, g_visualizer.starColor);
    if (n1>0) SDL_RenderPoints(ctx->renderer, pts, n1);
    if (n2>0) SDL_RenderFillRects(ctx->renderer, r2, n2);
    if (n3>0) SDL_RenderFillRects(ctx->renderer, r3, n3);

    int cx = TV_BOX_W / 2;
    int cy = TV_BOX_H / 2;
    int innerRadius = (TV_BOX_W < TV_BOX_H ? TV_BOX_W / 2 : TV_BOX_H / 2) - 56;
    if (innerRadius < 40) innerRadius = 40;

    if (bloom_ensure(ctx, TV_BOX_W, TV_BOX_H)) {
        float bs = 1.0f;
        memset(g_bloom.src, 0, g_bloom.width * g_bloom.height * 4);
        
        float auraStrength = clampf_local((g_radial.energy * 0.95f + g_radial.bass * 1.1f + g_radial.beatFlash * 0.55f) * g_visualizer.globalGlowStrength, 0.0f, 2.2f);
        float hueBase = fmodf((float)(g_radial.rotation * 0.02f * g_visualizer.colorCycleSpeed), 1.0f);
        if (hueBase < 0) hueBase += 1.0f;

        int auraRx = (int)lroundf(((double)innerRadius + 105.0) * (double)bs);
        int auraRy = (int)lroundf((((double)innerRadius * 0.78) + 68.0) * (double)bs);

        buffer_add_soft_ellipse(g_bloom.src, g_bloom.width, g_bloom.height, cx*bs, cy*bs, (float)auraRx, (float)auraRy, hsv_to_rgb(hueBase + 0.02f, 0.62f, 1.0f), auraStrength * 0.22f);
        buffer_add_soft_ellipse(g_bloom.src, g_bloom.width, g_bloom.height, cx*bs, cy*bs, (float)auraRx*0.72f, (float)auraRy*0.72f, hsv_to_rgb(hueBase + 0.16f, 0.68f, 1.00f), auraStrength * 0.32f);
        buffer_add_soft_ellipse(g_bloom.src, g_bloom.width, g_bloom.height, cx*bs, cy*bs, (float)auraRx*0.48f, (float)auraRy*0.48f, hsv_to_rgb(hueBase + 0.28f, 0.50f, 1.00f), auraStrength * 0.40f);

        for (int i = 0; i < BAND_COUNT; ++i) {
            float t = (float)i / BAND_COUNT;
            float angle = (float)(g_radial.rotation + (t * 2.0 * M_PI) - M_PI/2.0);
            float amp = clampf_local(g_radial.bands[i], 0, 1);
            float hiLenBoost = 1.0f + (powf(t, 1.6f) * g_visualizer.highHzLengthBonus);
            float len = 5.0f + ((((amp * 86.0f) + (g_radial.bass * 8.0f)) * 0.5f) * g_visualizer.barLengthScale * hiLenBoost);
            
            float sx = (cx + cosf(angle) * (float)innerRadius) * bs;
            float sy = (cy + sinf(angle) * (float)innerRadius) * bs;
            float ex = (cx + cosf(angle) * ((float)innerRadius + len)) * bs;
            float ey = (cy + sinf(angle) * ((float)innerRadius + len)) * bs;
            
            HP_Color col = hsv_to_rgb(fmodf(t + hueBase, 1.0f), 0.76f, 1.00f);
            float radiusF = (3.2f + amp * 5.2f + g_visualizer.globalGlowStrength * 2.8f) * bs;
            float intensity = (0.12f + amp * 0.28f + g_radial.beatFlash * 0.10f) * g_visualizer.globalGlowStrength;

            buffer_add_soft_line(g_bloom.src, g_bloom.width, g_bloom.height, sx, sy, ex, ey, radiusF, col, intensity);
        }

        int br = clampf_local((5.0f + g_visualizer.globalGlowStrength * 5.0f) * bs, 1, 10);
        blur_horizontal(g_bloom.src, g_bloom.tmp, g_bloom.width, g_bloom.height, br);
        blur_vertical_to_premult(g_bloom.tmp, (unsigned int*)g_bloom.pixels, g_bloom.width, g_bloom.height, br, 1.0f);
        SDL_UpdateTexture(g_bloom.texture, NULL, g_bloom.pixels, g_bloom.width * 4);
        
        SDL_SetTextureBlendMode(g_bloom.texture, SDL_BLENDMODE_ADD);
        HP_Texture hpt = {g_bloom.texture, g_bloom.width, g_bloom.height};
        hp_draw_texture(ctx, &hpt, NULL, &box, 255);
    }

    hp_draw_set_blend_mode(ctx, HP_BLEND_ALPHA);
    for (int i = 0; i < BAND_COUNT; ++i) {
        float t = (float)i / BAND_COUNT;
        float angle = (float)(g_radial.rotation + (t * 2.0 * M_PI) - M_PI/2.0);
        float amp = clampf_local(g_radial.bands[i], 0, 1);
        float hiLenBoost = 1.0f + (powf(t, 1.6f) * g_visualizer.highHzLengthBonus);
        float len = 5.0f + ((((amp * 86.0f) + (g_radial.bass * 8.0f)) * 0.5f) * g_visualizer.barLengthScale * hiLenBoost);
        
        float sx = (float)TV_BOX_X + (float)cx + cosf(angle) * (float)innerRadius;
        float sy = (float)TV_BOX_Y + (float)cy + sinf(angle) * (float)innerRadius;
        float ex = (float)TV_BOX_X + (float)cx + cosf(angle) * ((float)innerRadius + len);
        float ey = (float)TV_BOX_Y + (float)cy + sinf(angle) * ((float)innerRadius + len);

        hp_draw_set_color(ctx, hsv_to_rgb(fmodf(t + (float)(g_radial.rotation * 0.02f * g_visualizer.colorCycleSpeed), 1.0f), 0.84f, 0.40f + amp * 0.60f + g_radial.beatFlash * 0.04f));
        
        int width = 3;
        if (amp > 0.40f) width = 4;
        if (amp > 0.78f) width = 5;
        
        if (width == 3) {
            hp_draw_line(ctx, (int)sx, (int)sy, (int)ex, (int)ey);
        } else {
            for (int off = -width/2; off <= width/2; off++) {
                hp_draw_line(ctx, (int)sx+off, (int)sy, (int)ex+off, (int)ey);
            }
        }
    }
}
