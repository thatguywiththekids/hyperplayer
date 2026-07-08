#ifndef PORTABILITY_H
#define PORTABILITY_H

#ifdef _WIN32
#include <windows.h>
#else

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

// Basic Win32-style type aliases used in the core code
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef uint64_t ULONGLONG;
typedef intptr_t INT_PTR;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HCURSOR;

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

#endif // PORTABILITY_H
