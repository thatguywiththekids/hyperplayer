#include "spectrumanalyzer.h"
#include "player.h"
#include "renderer.h"
#include "portability.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

#define BOX_X 1363
#define BOX_Y 658
#define BOX_W 555
#define BOX_H 110

typedef struct SpectrumAnalyzerConfig {
    int fftSize;
    int bandCount;
    double minHz;
    double maxHz;
    int barW;
    int gapX;
    int slatH;
    int slatGap;
    HP_Color barColor;
} SpectrumAnalyzerConfig;

static double *g_window = NULL;
static double *g_real = NULL;
static double *g_imag = NULL;
static double *g_mags = NULL;
static float *g_mono = NULL;
static float *g_bars = NULL;
static int *g_bandEdges = NULL;
static double *g_bandEQ = NULL;
static bool g_initialized = false;
static bool g_configLoaded = false;
static int g_sampleRate = 44100;

static SpectrumAnalyzerConfig g_cfg = {
    1024,
    37,
    80.0,
    11000.0,
    14,
    1,
    6,
    2,
    {187, 187, 187, 255}
};

static double clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

static double lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

static HP_Color parse_hex_color(const wchar_t *text, HP_Color fallback)
{
    wchar_t *endPtr;
    unsigned long value;
    if (!text || wcslen(text) != 6) return fallback;

    value = wcstoul(text, &endPtr, 16);
    if (endPtr == text || *endPtr != L'\0' || value > 0xFFFFFFUL) return fallback;

    return hp_color_rgb((uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 8) & 0xFF), (uint8_t)(value & 0xFF));
}

static void build_ini_path(wchar_t *path, size_t pathCount)
{
    DWORD len;
    if (!path || pathCount == 0) return;

    len = GetModuleFileNameW(NULL, path, (DWORD)pathCount);
    if (len == 0 || len >= pathCount) {
        wcsncpy(path, L"hyperplayer.ini", pathCount);
        path[pathCount - 1] = L'\0';
        return;
    }

    for (size_t i = (size_t)len; i > 0; --i) {
        if (path[i - 1] == L'\\' || path[i - 1] == L'/') {
            wcsncpy(path + i, L"hyperplayer.ini", pathCount - i);
            path[pathCount - 1] = L'\0';
            return;
        }
    }
    wcsncpy(path, L"hyperplayer.ini", pathCount);
    path[pathCount - 1] = L'\0';
}

static int normalize_fft_size(int value)
{
    int normalized = 64;
    if (value < 64) return 64;
    if (value > 8192) value = 8192;
    while (normalized < value && normalized < 8192) normalized <<= 1;
    return normalized;
}

static void load_config_from_ini(void)
{
    wchar_t iniPath[MAX_PATH];
    wchar_t value[64];

    if (g_configLoaded) return;
    build_ini_path(iniPath, sizeof(iniPath) / sizeof(iniPath[0]));

    g_cfg.fftSize = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"FFT_SIZE", g_cfg.fftSize, iniPath);
    g_cfg.bandCount = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"BAND_COUNT", g_cfg.bandCount, iniPath);
    
    GetPrivateProfileStringW(L"SPECTRUMANALYZER", L"MIN_HZ", L"", value, 64, iniPath);
    if (value[0] != L'\0') g_cfg.minHz = wcstod(value, NULL);
    
    GetPrivateProfileStringW(L"SPECTRUMANALYZER", L"MAX_HZ", L"", value, 64, iniPath);
    if (value[0] != L'\0') g_cfg.maxHz = wcstod(value, NULL);

    g_cfg.barW = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"BAR_W", g_cfg.barW, iniPath);
    g_cfg.gapX = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"GAP_X", g_cfg.gapX, iniPath);
    g_cfg.slatH = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"SLAT_H", g_cfg.slatH, iniPath);
    g_cfg.slatGap = GetPrivateProfileIntW(L"SPECTRUMANALYZER", L"SLAT_GAP", g_cfg.slatGap, iniPath);

    GetPrivateProfileStringW(L"SPECTRUMANALYZER", L"BAR_COLOR", L"", value, 64, iniPath);
    if (value[0] != L'\0') g_cfg.barColor = parse_hex_color(value, g_cfg.barColor);

    // Normalization & Range Checks
    g_cfg.fftSize = normalize_fft_size(g_cfg.fftSize);
    if (g_cfg.bandCount < 2) g_cfg.bandCount = 2;
    if (g_cfg.bandCount > 256) g_cfg.bandCount = 256;
    if (g_cfg.minHz < 1.0) g_cfg.minHz = 1.0;
    if (g_cfg.maxHz <= g_cfg.minHz) { g_cfg.minHz = 80.0; g_cfg.maxHz = 11000.0; }
    if (g_cfg.barW < 1) g_cfg.barW = 1;
    if (g_cfg.gapX < 0) g_cfg.gapX = 0;
    if (g_cfg.slatH < 1) g_cfg.slatH = 1;
    if (g_cfg.slatGap < 0) g_cfg.slatGap = 0;

    g_configLoaded = true;
}

static void release_tables(void)
{
    free(g_window);
    free(g_real);
    free(g_imag);
    free(g_mags);
    free(g_mono);
    free(g_bars);
    free(g_bandEdges);
    free(g_bandEQ);

    g_window = NULL; g_real = NULL; g_imag = NULL; g_mags = NULL;
    g_mono = NULL; g_bars = NULL; g_bandEdges = NULL; g_bandEQ = NULL;
}

static bool allocate_tables(void)
{
    size_t fftCount = (size_t)g_cfg.fftSize;
    size_t halfCount = (size_t)(g_cfg.fftSize / 2);
    size_t bandCount = (size_t)g_cfg.bandCount;

    g_window = (double *)malloc(sizeof(double) * fftCount);
    g_real = (double *)malloc(sizeof(double) * fftCount);
    g_imag = (double *)malloc(sizeof(double) * fftCount);
    g_mags = (double *)malloc(sizeof(double) * halfCount);
    g_mono = (float *)malloc(sizeof(float) * fftCount);
    g_bars = (float *)calloc(bandCount, sizeof(float));
    g_bandEdges = (int *)malloc(sizeof(int) * (bandCount + 1));
    g_bandEQ = (double *)malloc(sizeof(double) * bandCount);

    if (!g_window || !g_real || !g_imag || !g_mags || !g_mono || !g_bars || !g_bandEdges || !g_bandEQ) {
        release_tables();
        return false;
    }
    return true;
}

static int reverse_bits(int x, int bits)
{
    int y = 0;
    for (int i = 0; i < bits; ++i) {
        y = (y * 2) + (x & 1);
        x >>= 1;
    }
    return y;
}

static void fft_inplace(double *re, double *im, int n)
{
    int bits = 0;
    int temp = n;
    while (temp > 1) { bits++; temp >>= 1; }

    for (int i = 0; i < n; ++i) {
        int j = reverse_bits(i, bits);
        if (j > i) {
            double tr = re[i]; double ti = im[i];
            re[i] = re[j]; im[i] = im[j];
            re[j] = tr; im[j] = ti;
        }
    }

    for (int len = 2; len <= n; len *= 2) {
        double ang = -2.0 * 3.14159265358979323846 / (double)len;
        double wlenCos = cos(ang);
        double wlenSin = sin(ang);

        for (int i = 0; i < n; i += len) {
            double wCos = 1.0;
            double wSin = 0.0;
            int half = len / 2;

            for (int j = 0; j < half; ++j) {
                double uR = re[i + j];
                double uI = im[i + j];
                double vR = (re[i + j + half] * wCos) - (im[i + j + half] * wSin);
                double vI = (re[i + j + half] * wSin) + (im[i + j + half] * wCos);

                re[i + j] = uR + vR;
                im[i + j] = uI + vI;
                re[i + j + half] = uR - vR;
                im[i + j + half] = uI - vI;

                double nextCos = (wCos * wlenCos) - (wSin * wlenSin);
                double nextSin = (wCos * wlenSin) + (wSin * wlenCos);
                wCos = nextCos; wSin = nextSin;
            }
        }
    }
}

static int hz_to_bin(double hz)
{
    int bin = (int)floor(((hz / (double)g_sampleRate) * (double)g_cfg.fftSize) + 0.5);
    int half = g_cfg.fftSize / 2;
    if (bin < 1) bin = 1;
    if (bin > half) bin = half;
    return bin;
}

static void init_tables(void)
{
    if (g_initialized) return;
    load_config_from_ini();
    if (!allocate_tables()) return;

    for (int n = 0; n < g_cfg.fftSize; ++n) {
        g_window[n] = 0.5 * (1.0 - cos((2.0 * 3.14159265358979323846 * (double)n) / (double)(g_cfg.fftSize - 1)));
    }

    double logMin = log(g_cfg.minHz);
    double logMax = log(g_cfg.maxHz);

    for (int i = 0; i <= g_cfg.bandCount; ++i) {
        double t = (double)i / (double)g_cfg.bandCount;
        double hz = exp(logMin + ((logMax - logMin) * t));
        g_bandEdges[i] = hz_to_bin(hz);
    }

    for (int i = 0; i < g_cfg.bandCount; ++i) {
        double t = (double)i / (double)(g_cfg.bandCount - 1);
        g_bandEQ[i] = lerp(0.24, 0.50, t);
    }
    g_initialized = true;
}

void spectrumanalyzer_update(AppState *app, double dt)
{
    (void)dt;
    init_tables();
    if (!g_initialized || !g_bars) return;

    if (!player_get_recent_mono_window(app, g_mono, g_cfg.fftSize)) return;

    for (int i = 0; i < g_cfg.fftSize; ++i) {
        g_real[i] = (double)g_mono[i] * g_window[i];
        g_imag[i] = 0.0;
    }

    fft_inplace(g_real, g_imag, g_cfg.fftSize);

    int half = g_cfg.fftSize / 2;
    for (int k = 0; k < half; ++k) {
        double r = g_real[k];
        double im = g_imag[k];
        g_mags[k] = sqrt((r * r) + (im * im)) + 1e-12;
    }

    for (int i = 0; i < g_cfg.bandCount; ++i) {
        int a = g_bandEdges[i];
        int b = g_bandEdges[i + 1] - 1;
        double peak = 0.0, sum = 0.0;
        int count = 0;

        if (b < a) b = a;
        if (b > half) b = half;

        for (int k = a; k <= b; ++k) {
            double m = g_mags[k - 1];
            sum += m; count++;
            if (m > peak) peak = m;
        }

        double avg = (count > 0) ? (sum / (double)count) : 0.0;
        double raw = (peak * 0.65) + (avg * 0.35);
        raw = raw * g_bandEQ[i];
        raw = log(1.0 + (raw * 3.0)) / log(4.0);
        raw = clamp01(raw * 0.38);

        float prev = g_bars[i];
        if ((float)raw > prev) g_bars[i] = (float)lerp(prev, raw, 0.68);
        else g_bars[i] = (float)lerp(prev, raw, 0.24);
    }
}

void spectrumanalyzer_draw(AppState *app, HP_DrawContext *ctx)
{
    const int bottom = BOX_Y + BOX_H;
    const int maxH = BOX_H;
    const int barStep = g_cfg.barW + g_cfg.gapX;
    const int stepY = g_cfg.slatH + g_cfg.slatGap;

    (void)app;
    if (!ctx) return;
    init_tables();
    if (!g_initialized || !g_bars) return;

    hp_draw_set_color(ctx, g_cfg.barColor);

    for (int i = 0; i < g_cfg.bandCount; ++i) {
        int h = (int)floor((double)g_bars[i] * (double)maxH + 0.5);
        if (h > 0) {
            int x = BOX_X + (i * barStep);
            int drawn = 0;
            while (drawn < h) {
                int piece = g_cfg.slatH;
                if (drawn + piece > h) piece = h - drawn;
                int y = bottom - drawn - piece;
                HP_Rect r = { x, y, g_cfg.barW, piece };
                hp_draw_fill_rect(ctx, &r);
                drawn += stepY;
            }
        }
    }
}
