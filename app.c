#include "app.h"
#include "ui.h"
#include "renderer.h"
#ifdef _WIN32
#include "directory_listing_win32.h"
#include "resource.h"
#else
#include "directory_listing.h"
#endif
#include "player.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static void app_copy_wstr(wchar_t *dst, size_t dstCount, const wchar_t *src)
{
    if (!dst || dstCount == 0) return;
    if (!src) { dst[0] = L'\0'; return; }
    wcsncpy(dst, src, dstCount - 1);
    dst[dstCount - 1] = L'\0';
}

void app_join_path(wchar_t *dst, size_t dstCount, const wchar_t *dir, const wchar_t *name)
{
    if (!dst || dstCount == 0) return;
    dst[0] = L'\0';
    if (!dir || !name) return;
#ifdef _WIN32
    swprintf(dst, dstCount - 1, L"%ls\\%ls", dir, name);
#else
    swprintf(dst, dstCount - 1, L"%ls/%ls", dir, name);
#endif
    dst[dstCount - 1] = L'\0';
}

static void app_get_exe_dir(wchar_t *dst, size_t dstCount)
{
    wchar_t fullPath[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, fullPath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) { app_copy_wstr(dst, dstCount, L"."); return; }
    fullPath[len] = L'\0';
    for (int i = (int)len - 1; i >= 0; --i) {
        if (fullPath[i] == L'\\' || fullPath[i] == L'/') { fullPath[i] = L'\0'; break; }
    }
    app_copy_wstr(dst, dstCount, fullPath);
}

static void app_trim_wstr_in_place(wchar_t *text)
{
    if (!text) return;
    size_t len = wcslen(text), start = 0, end = len;
    while (start < len && iswspace(text[start])) start++;
    while (end > start && iswspace(text[end - 1])) end--;
    if (start > 0) memmove(text, text + start, (end - start) * sizeof(wchar_t));
    text[end - start] = L'\0';
}

static void app_get_active_ini_path(const AppState *app, wchar_t *dst, size_t dstCount)
{
    if (!dst || dstCount == 0) return;
    if (app && app->iniPath[0] != L'\0') { app_copy_wstr(dst, dstCount, app->iniPath); return; }
    wchar_t exeDir[MAX_PATH];
    app_get_exe_dir(exeDir, MAX_PATH);
    app_join_path(dst, dstCount, exeDir, L"hyperplayer.ini");
}

static bool app_parse_hex_color_text(const wchar_t *text, HP_Color *outColor)
{
    wchar_t cleaned[32]; size_t di = 0; wchar_t *endPtr = NULL;
    if (!text || !outColor) return false;
    while (*text && iswspace(*text)) text++;
    if (*text == L'#') text++;
    else if (text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) text += 2;
    while (*text && di < 31) {
        if (*text == L';') break;
        if (!iswspace(*text)) cleaned[di++] = *text;
        text++;
    }
    cleaned[di] = L'\0';
    if (di != 6) return false;
    unsigned long val = wcstoul(cleaned, &endPtr, 16);
    if (!endPtr || *endPtr != L'\0') return false;
    *outColor = (HP_Color){(uint8_t)((val>>16)&0xFF), (uint8_t)((val>>8)&0xFF), (uint8_t)(val&0xFF), 255};
    return true;
}

static void app_color_to_hex_text(HP_Color color, wchar_t *dst, size_t dstCount)
{
    if (!dst || dstCount == 0) return;
    swprintf(dst, dstCount - 1, L"%02X%02X%02X", color.r, color.g, color.b);
    dst[dstCount - 1] = L'\0';
}

bool app_ensure_default_ini_exists(void)
{
    wchar_t exeDir[MAX_PATH], iniPath[MAX_PATH];
    char mbsPath[MAX_PATH*4];
    app_get_exe_dir(exeDir, MAX_PATH);
    app_join_path(iniPath, MAX_PATH, exeDir, L"hyperplayer.ini");
    
    wcstombs(mbsPath, iniPath, sizeof(mbsPath));
    FILE *f = fopen(mbsPath, "rb"); if (f) { fclose(f); return true; }
    f = fopen(mbsPath, "wb"); if (!f) return false;
    extern const char g_defaultIniFileContents[];
    fwrite(g_defaultIniFileContents, 1, strlen(g_defaultIniFileContents), f);
    fclose(f); return true;
}

bool app_ini_get_string(const AppState *app, const wchar_t *section, const wchar_t *key, const wchar_t *defaultValue, wchar_t *dst, size_t dstCount)
{
    wchar_t iniPath[MAX_PATH];
    if (!dst || dstCount == 0) return false;
    dst[0] = L'\0';
    if (!section || !key) {
        if (defaultValue) { app_copy_wstr(dst, dstCount, defaultValue); app_trim_wstr_in_place(dst); }
        return false;
    }
    app_get_active_ini_path(app, iniPath, MAX_PATH);
    
    // If GetPrivateProfileStringW returns 0 and it wasn't an empty string read, it failed
    if (GetPrivateProfileStringW(section, key, L"", dst, (DWORD)dstCount, iniPath) == 0) {
        if (defaultValue) { app_copy_wstr(dst, dstCount, defaultValue); app_trim_wstr_in_place(dst); }
        return false;
    }

    app_trim_wstr_in_place(dst);
    return (dst[0] != L'\0');
}

int app_ini_get_int(const AppState *app, const wchar_t *section, const wchar_t *key, int defaultValue)
{
    wchar_t text[64];
    if (!app_ini_get_string(app, section, key, NULL, text, 64)) return defaultValue;
    return (int)wcstol(text, NULL, 10);
}

double app_ini_get_double(const AppState *app, const wchar_t *section, const wchar_t *key, double defaultValue)
{
    wchar_t text[64];
    if (!app_ini_get_string(app, section, key, NULL, text, 64)) return defaultValue;
    return wcstod(text, NULL);
}

HP_Color app_ini_get_color(const AppState *app, const wchar_t *section, const wchar_t *key, HP_Color defaultValue)
{
    wchar_t text[64]; HP_Color color;
    if (!app_ini_get_string(app, section, key, NULL, text, 64)) return defaultValue;
    if (!app_parse_hex_color_text(text, &color)) return defaultValue;
    return color;
}

static void app_load_config(AppState *app)
{
    if (!app) return;
    app_ini_get_string(app, L"SYSTEM", L"DEFAULTDIR", L".", app->config.defaultDir, MAX_PATH);
    app->config.stereoSeparation = app_ini_get_int(app, L"AUDIO", L"STEREOSEPARATION", 33);
    if (app->config.stereoSeparation < 0) app->config.stereoSeparation = 0;
    if (app->config.stereoSeparation > 200) app->config.stereoSeparation = 200;
    app->config.modOctaveOffset = app_ini_get_int(app, L"PATTERN", L"MOD_OCTAVE_OFFSET", 0);
}

bool app_prepare_runtime_dlls(AppState *app) { if (app) app->runtimeDllsReady = true; return true; }
void app_cleanup_runtime_dlls(AppState *app) { (void)app; }

void app_set_status(AppState *app, const wchar_t *fmt, ...)
{
    va_list args; if (!app || !fmt) return;
    va_start(args, fmt);
    vswprintf(app->statusText, 2048, fmt, args);
    va_end(args);
}

static void app_resolve_default_dir(const AppState *app, wchar_t *dst, size_t dstCount)
{
    if (!dst || dstCount == 0) return;
    dst[0] = L'\0'; if (!app) return;
    if (app->config.defaultDir[0] == L'\0' || (app->config.defaultDir[0] == L'.' && app->config.defaultDir[1] == L'\0')) {
        app_copy_wstr(dst, dstCount, app->exeDir); return;
    }
    // Simple absolute path check
    if (app->config.defaultDir[0] == L'/' || (app->config.defaultDir[0] && app->config.defaultDir[1] == L':')) {
        app_copy_wstr(dst, dstCount, app->config.defaultDir); return;
    }
    app_join_path(dst, dstCount, app->exeDir, app->config.defaultDir);
}

bool app_init(AppState *app)
{
    if (!app) return false;
    
    app_ensure_default_ini_exists();

    app_get_exe_dir(app->exeDir, MAX_PATH);
    app_join_path(app->iniPath, MAX_PATH, app->exeDir, L"hyperplayer.ini");
    app_join_path(app->backgroundPath, MAX_PATH, app->exeDir, L"background.png");
    app_join_path(app->fontPath, MAX_PATH, app->exeDir, L"protracker.ttf");
    
    app->currentSelectedFile[0] = L'\0'; app->currentSelectedName[0] = L'\0';
    app->selectedSampleIndex = 0; app->sampleDisplayCurrentSlot = 0; app->sampleDisplayTimer = 0.0;
    app->pendingWheelDelta = 0; app->lastUpdateTick = GetTickCount64(); app->showFileBrowser = true;

    for (int i = 0; i < 31; ++i) app->sampleHighlightAlpha[i] = 0.0f;

    app_load_config(app);
    player_init(app);

    wchar_t defaultDir[MAX_PATH];
    app_resolve_default_dir(app, defaultDir, MAX_PATH);
    directory_listing_init(app, defaultDir);

    app_set_status(app, L"Ready.");
    return true;
}

void app_shutdown(AppState *app)
{
    if (!app) return;
    directory_listing_shutdown(app);
    player_shutdown(app);
    ui_release_assets(app);
}

const char g_defaultIniFileContents[] =
    "[SYSTEM]\r\nDEFAULTDIR=.\r\n\r\n"
    "[AUDIO]\r\nSTEREOSEPARATION=33\r\n\r\n"
    "[SAMPLELIST]\r\nBGTRANSPARENCY=45\r\nBGFADE=0.95\r\n\r\n"
    "[PATTERN]\r\nTEXTCOLOR1=3648FF\r\nTEXTCOLOR2=7196FF\r\n\r\n"
    "[QUADRASCOPE]\r\nQUADRACOLOR=FFDD00\r\n\r\n"
    "[VUMETER]\r\nVUCOLOR1=00FF00\r\nVUCOLOR2=FFFF00\r\nVUCOLOR3=FF0000\r\nVUTRANSPARENCY=45\r\n\r\n"
    "[SAMPLEVIEW]\r\nSAMPLECOLOR=FFDD00\r\n\r\n"
    "[SPECTRUMANALYZER]\r\nFFT_SIZE=1024\r\nBAND_COUNT=37\r\nMIN_HZ=80.0\r\nMAX_HZ=11000.0\r\nBAR_W=14\r\nGAP_X=1\r\nSLAT_H=6\r\nSLAT_GAP=2\r\n\r\n"
    "[VISUALIZER]\r\nVIS_ROTATION_SPEED=2.00\r\nVIS_BAR_DECLINE_SPEED=3.60\r\nVIS_BAR_LENGTH_SCALE=1.30\r\nVIS_GLOBAL_GLOW_STRENGTH=0.90\r\nVIS_HIGH_HZ_RESPONSE=2.00\r\nVIS_HIGH_HZ_LENGTH_BONUS=1.63\r\nVIS_STARFIELD_COUNT=5000\r\nVIS_STARFIELD_SPEED=0.36\r\nVIS_STARFIELD_BRIGHTNESS=1.00\r\nVIS_BASS_ONLY_BAND_COUNT=20\r\nVIS_BASS_ONLY_DOMINANCE=1.10\r\nVIS_BASS_ONLY_GATE_SCALE=0.85\r\nVIS_BASS_ONLY_ABS_LEVEL=0.018\r\nVIS_AGC_IGNORE_LOW_BANDS=20\r\nVIS_HIGH_PRESENCE_FLOOR=0.90\r\n";
