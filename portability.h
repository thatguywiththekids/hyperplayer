#ifndef PORTABILITY_H
#define PORTABILITY_H

#ifdef _WIN32
#include <windows.h>
#else

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

// Win32 Types
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HANDLE;
typedef void* HMODULE;
typedef uint32_t COLORREF;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef void* HFONT;
typedef void* HBITMAP;
typedef void* HCURSOR;
typedef void* HPEN;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef void* HRSRC;
typedef void* HGLOBAL;
typedef int32_t HRESULT;
typedef uintptr_t ULONG_PTR;
typedef void* PVOID;
typedef void* HBRUSH;
typedef intptr_t INT_PTR;

typedef struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

typedef struct POINT {
    LONG x;
    LONG y;
} POINT;

typedef struct SIZE {
    LONG cx;
    LONG cy;
} SIZE;

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb)>>16))

#define MAX_PATH 260
#define GWLP_USERDATA (-21)

// GDI equivalents
typedef struct HDC_REC {
    SDL_Renderer *renderer;
    SDL_Color textColor;
    SDL_Color penColor;
    TTF_Font *font;
    SDL_Texture *target;
    SDL_Texture *savedTarget;
    void *selectedBitmap;
    POINT currentPos;
} *HDC;

// Mock some Win32 macros
#ifndef _snwprintf
#define _snwprintf swprintf
#endif
#define lstrcpynW(dst, src, count) wcsncpy(dst, src, count)
#define lstrlenW(src) wcslen(src)
#define lstrcatW(dst, src) wcscat(dst, src)

#define LOWORD(l)           ((uint16_t)(((uint32_t)(l)) & 0xffff))
#define HIWORD(l)           ((uint16_t)((((uint32_t)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((uint8_t)(((uint32_t)(w)) & 0xff))
#define HIBYTE(w)           ((uint8_t)((((uint32_t)(w)) >> 8) & 0xff))

#define GET_WHEEL_DELTA_WPARAM(wparam) ((int16_t)HIWORD(wparam))

#define CALLBACK
#define WINAPI

#define FALSE 0
#define TRUE 1

#define ERROR_ALREADY_EXISTS 183L
#define RT_RCDATA ((const wchar_t*)10)
#define GENERIC_WRITE (0x40000000L)
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010

#define S_OK ((HRESULT)0L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define RPC_E_CHANGED_MODE ((HRESULT)0x80010106L)
#define COINIT_APARTMENTTHREADED 0x2

#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define NONANTIALIASED_QUALITY 3
#define DEFAULT_QUALITY 0
#define FF_DONTCARE 0
#define FW_NORMAL 400
#define FW_BOLD 700

#define TRANSPARENT 1
#define HALFTONE 4
#define BI_RGB 0
#define DIB_RGB_COLORS 0
#define SRCCOPY 0x00CC0020

#define DT_LEFT 0x00000000
#define DT_RIGHT 0x00000002
#define DT_TOP 0x00000000
#define DT_VCENTER 0x00000004
#define DT_SINGLELINE 0x00000020
#define DT_NOPREFIX 0x00000800
#define DT_END_ELLIPSIS 0x00004000

#define AC_SRC_OVER 0x00
#define AC_SRC_ALPHA 0x01
#define WHITE_BRUSH 0
#define NULL_PEN 1

#define PS_SOLID 0
#define PS_GEOMETRIC 0x00010000
#define PS_ENDCAP_ROUND 0x00000000
#define PS_JOIN_ROUND 0x00000000
#define BS_SOLID 0
#define SW_SHOWNORMAL 1

typedef struct LOGBRUSH {
    UINT     lbStyle;
    COLORREF lbColor;
    ULONG_PTR lbHatch;
} LOGBRUSH;

typedef struct BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

typedef struct BITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct BITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    COLORREF         bmiColors[1];
} BITMAPINFO;

#define MAKEINTRESOURCEW(i) ((wchar_t*)((ULONG_PTR)((WORD)(i))))

// Functions to be implemented
uint64_t GetTickCount64(void);
void Sleep(uint32_t ms);
void SetPortabilityTime(uint64_t now);
DWORD GetModuleFileNameW(void* hModule, wchar_t* lpFilename, DWORD nSize);
bool CreateDirectoryW(const wchar_t* lpPathName, void* lpSecurityAttributes);
DWORD GetLastError(void);
void* FindResourceW(void* hModule, const wchar_t* lpName, const wchar_t* lpType);
DWORD SizeofResource(void* hModule, void* hResInfo);
void* LoadResource(void* hModule, void* hResInfo);
void* LockResource(void* hResData);
HANDLE CreateFileW(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
bool WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* lpOverlapped);
bool CloseHandle(HANDLE hObject);
bool DeleteFileW(const wchar_t* lpFileName);
DWORD GetFileAttributesW(const wchar_t* lpFileName);
DWORD GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName, const wchar_t* lpDefault, wchar_t* lpReturnedString, DWORD nSize, const wchar_t* lpFileName);
int GetPrivateProfileIntW(const wchar_t* lpAppName, const wchar_t* lpKeyName, int nDefault, const wchar_t* lpFileName);
DWORD GetTempPathW(DWORD nBufferLength, wchar_t* lpBuffer);
uint32_t GetCurrentProcessId(void);
bool RemoveDirectoryW(const wchar_t* lpPathName);
HRESULT CoInitializeEx(void* pvReserved, uint32_t dwCoInit);
void CoUninitialize(void);

HFONT CreateFontW(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily, const wchar_t* lpszFace);
void DeleteObject(void* ho);
void* SelectObject(HDC hdc, void* h);
int SetBkMode(HDC hdc, int mode);
COLORREF SetTextColor(HDC hdc, COLORREF color);
bool TextOutW(HDC hdc, int x, int y, const wchar_t* lpString, int c);
bool GetTextExtentPoint32W(HDC hdc, const wchar_t* lpString, int c, SIZE* psiz);
int DrawTextW(HDC hdc, wchar_t* lpchText, int cchText, RECT* lprc, UINT format);
int SaveDC(HDC hdc);
bool RestoreDC(HDC hdc, int nSavedDC);
bool IntersectClipRect(HDC hdc, int left, int top, int right, int bottom);
HBRUSH CreateSolidBrush(COLORREF color);
void FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr);
int SetStretchBltMode(HDC hdc, int mode);
HPEN CreatePen(int fnPenStyle, int nWidth, COLORREF crColor);
HPEN ExtCreatePen(DWORD dwPenStyle, DWORD dwWidth, const LOGBRUSH* lplb, DWORD dwStyleCount, const DWORD* lpStyle);
bool Polyline(HDC hdc, const POINT* lppt, int cPoints);
HDC GetDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC hdc);
bool MoveToEx(HDC hdc, int x, int y, POINT* lppt);
bool LineTo(HDC hdc, int x, int y);
bool SetPixelV(HDC hdc, int x, int y, COLORREF color);
bool DeleteDC(HDC hdc);
HDC CreateCompatibleDC(HDC hdc);
HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE hSection, DWORD offset);
void* GetStockObject(int fnObject);
bool AlphaBlend(HDC hdcDest, int xoriginDest, int yoriginDest, int wdest, int hdest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wsrc, int hsrc, BLENDFUNCTION ftn);
void* ShellExecuteW(HWND hwnd, const wchar_t* lpOperation, const wchar_t* lpFile, const wchar_t* lpParameters, const wchar_t* lpDirectory, int nShowCmd);

#endif // _WIN32

#endif // PORTABILITY_H
