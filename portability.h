#ifndef PORTABILITY_H
#define PORTABILITY_H

#ifdef _WIN32
#include <windows.h>
#undef interface
#else

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

// Basic Win32-style type aliases used in the core code
typedef uint32_t DWORD;
typedef void* HANDLE;
typedef void* HWND;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// System shims for Linux/POSIX
uint64_t GetTickCount64(void);
DWORD GetModuleFileNameW(HANDLE hModule, wchar_t* lpFilename, DWORD nSize);
DWORD GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName, const wchar_t* lpDefault, wchar_t* lpReturnedString, DWORD nSize, const wchar_t* lpFileName);

#define SW_SHOWNORMAL 1
void* ShellExecuteW(HWND hwnd, const wchar_t* lpOperation, const wchar_t* lpFile, const wchar_t* lpParameters, const wchar_t* lpDirectory, int nShowCmd);

#endif // _WIN32

#include <stdio.h>
#include <wchar.h>
#include <stddef.h>

void hp_wstr_to_utf8(char *dst, size_t dstBytes, const wchar_t *src);
FILE *hp_fopen(const wchar_t *path, const wchar_t *mode);

#endif // PORTABILITY_H
