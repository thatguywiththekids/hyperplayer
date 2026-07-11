#include "portability.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

void hp_wstr_to_utf8(char *dst, size_t dstBytes, const wchar_t *src)
{
    if (!dst || dstBytes == 0) return;
    dst[0] = '\0';
    if (!src) return;
#ifdef _WIN32
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dstBytes, NULL, NULL);
#else
    wcstombs(dst, src, dstBytes);
#endif
}

FILE *hp_fopen(const wchar_t *path, const wchar_t *mode)
{
#ifdef _WIN32
    return _wfopen(path, mode);
#else
    char mbsPath[4096];
    char mbsMode[16];
    wcstombs(mbsPath, path, sizeof(mbsPath));
    wcstombs(mbsMode, mode, sizeof(mbsMode));
    return fopen(mbsPath, mbsMode);
#endif
}

#ifndef _WIN32

#include <time.h>
#include <unistd.h>
#include <SDL3/SDL.h>

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
