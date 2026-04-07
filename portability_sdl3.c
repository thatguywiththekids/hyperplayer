#include "portability.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <locale.h>

// Tagged GDI Objects
typedef enum { GDI_TYPE_FONT, GDI_TYPE_PEN, GDI_TYPE_BRUSH, GDI_TYPE_BITMAP } GdiType;
typedef struct {
    GdiType type;
    void *ptr;
    void *bits;
    int width;
    int height;
    COLORREF color; // For pens/brushes
    bool isStock;
} GdiObj;

static GdiObj* create_gdi_obj(GdiType type, void *ptr) {
    GdiObj *obj = (GdiObj*)malloc(sizeof(GdiObj));
    if (obj) {
        obj->type = type;
        obj->ptr = ptr;
        obj->bits = NULL;
        obj->width = 0;
        obj->height = 0;
        obj->color = 0;
        obj->isStock = false;
    }
    return obj;
}

// System functions
uint64_t GetTickCount64(void) { return SDL_GetTicks(); }
void Sleep(uint32_t ms) { SDL_Delay(ms); }

DWORD GetModuleFileNameW(void* hModule, wchar_t* lpFilename, DWORD nSize) {
    if (!lpFilename || nSize == 0) return 0;
    
    const char *path = SDL_GetBasePath();
    if (!path) return 0;
    
    size_t len = mbstowcs(lpFilename, path, nSize);
    
    if (len == (size_t)-1) {
        lpFilename[0] = L'\0';
        return 0;
    }
    
    if (len >= nSize) {
        lpFilename[nSize - 1] = L'\0';
        return nSize - 1;
    }
    
    return (DWORD)len;
}

bool CreateDirectoryW(const wchar_t* lpPathName, void* lpSecurityAttributes) {
    char path[MAX_PATH*4];
    wcstombs(path, lpPathName, sizeof(path));
    return mkdir(path, 0755) == 0;
}

DWORD GetLastError(void) { return 0; }
void* FindResourceW(void* hModule, const wchar_t* lpName, const wchar_t* lpType) { return NULL; }
DWORD SizeofResource(void* hModule, void* hResInfo) { return 0; }
void* LoadResource(void* hModule, void* hResInfo) { return NULL; }
void* LockResource(void* hResData) { return NULL; }

HANDLE CreateFileW(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    char path[MAX_PATH*4];
    wcstombs(path, lpFileName, sizeof(path));
    const char *mode = (dwDesiredAccess & GENERIC_WRITE) ? "wb" : "rb";
    return (HANDLE)fopen(path, mode);
}

bool WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* lpOverlapped) {
    if (!hFile) return false;
    size_t written = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, (FILE*)hFile);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)written;
    return written == nNumberOfBytesToWrite;
}

bool CloseHandle(HANDLE hObject) {
    if (!hObject) return false;
    return fclose((FILE*)hObject) == 0;
}

bool DeleteFileW(const wchar_t* lpFileName) {
    char path[MAX_PATH*4];
    wcstombs(path, lpFileName, sizeof(path));
    return remove(path) == 0;
}

DWORD GetFileAttributesW(const wchar_t* lpFileName) {
    char path[MAX_PATH*4];
    wcstombs(path, lpFileName, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    DWORD attr = FILE_ATTRIBUTE_NORMAL;
    if (S_ISDIR(st.st_mode)) attr |= FILE_ATTRIBUTE_DIRECTORY;
    return attr;
}

DWORD GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName, const wchar_t* lpDefault, wchar_t* lpReturnedString, DWORD nSize, const wchar_t* lpFileName) {
    if (!lpReturnedString || nSize == 0) return 0;
    if (lpDefault) {
        wcsncpy(lpReturnedString, lpDefault, nSize - 1);
        lpReturnedString[nSize - 1] = L'\0';
        return (DWORD)wcslen(lpReturnedString);
    }
    lpReturnedString[0] = L'\0';
    return 0;
}

DWORD GetTempPathW(DWORD nBufferLength, wchar_t* lpBuffer) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    return (DWORD)mbstowcs(lpBuffer, tmp, nBufferLength);
}

uint32_t GetCurrentProcessId(void) { return (uint32_t)getpid(); }
bool RemoveDirectoryW(const wchar_t* lpPathName) {
    char path[MAX_PATH*4];
    wcstombs(path, lpPathName, sizeof(path));
    return rmdir(path) == 0;
}

HRESULT CoInitializeEx(void* pvReserved, uint32_t dwCoInit) { return S_OK; }
void CoUninitialize(void) {}

// GDI Functions
HFONT CreateFontW(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily, const wchar_t* lpszFace) {
    TTF_Font *font = TTF_OpenFont("protracker.ttf", (float)abs(nHeight));
    if (!font) {
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", (float)abs(nHeight));
    }
    return (HFONT)create_gdi_obj(GDI_TYPE_FONT, font);
}

void DeleteObject(void* ho) {
    if (!ho) return;
    GdiObj *obj = (GdiObj*)ho;
    if (obj->isStock) return;

    if (obj->type == GDI_TYPE_FONT) {
        if (obj->ptr) TTF_CloseFont((TTF_Font*)obj->ptr);
    } else if (obj->type == GDI_TYPE_BITMAP) {
        if (obj->ptr) SDL_DestroyTexture((SDL_Texture*)obj->ptr);
        if (obj->bits) free(obj->bits);
    }
    free(obj);
}

void* SelectObject(HDC hdc, void* h) {
    if (!hdc || !h) return NULL;
    GdiObj *obj = (GdiObj*)h;
    void *old = h; 

    if (obj->type == GDI_TYPE_FONT) {
        hdc->font = (TTF_Font*)obj->ptr;
    } else if (obj->type == GDI_TYPE_PEN) {
        hdc->penColor.r = GetRValue(obj->color);
        hdc->penColor.g = GetGValue(obj->color);
        hdc->penColor.b = GetBValue(obj->color);
        hdc->penColor.a = 255;
    } else if (obj->type == GDI_TYPE_BRUSH) {
        hdc->penColor.r = GetRValue(obj->color);
        hdc->penColor.g = GetGValue(obj->color);
        hdc->penColor.b = GetBValue(obj->color);
        hdc->penColor.a = 255;
    } else if (obj->type == GDI_TYPE_BITMAP) {
        old = hdc->selectedBitmap;
        hdc->selectedBitmap = obj;
        if (hdc->target != obj->ptr) {
            hdc->target = (SDL_Texture*)obj->ptr;
            SDL_SetRenderTarget(hdc->renderer, hdc->target);
        }
    }
    return old;
}
int SetBkMode(HDC hdc, int mode) { return 0; }

COLORREF SetTextColor(HDC hdc, COLORREF color) {
    if (!hdc) return 0;
    COLORREF old = RGB(hdc->textColor.r, hdc->textColor.g, hdc->textColor.b);
    hdc->textColor.r = GetRValue(color);
    hdc->textColor.g = GetGValue(color);
    hdc->textColor.b = GetBValue(color);
    hdc->textColor.a = 255;
    return old;
}

bool TextOutW(HDC hdc, int x, int y, const wchar_t* lpString, int c) {
    if (!hdc || !hdc->font || !lpString) return false;
    
    wchar_t buf[4096];
    const wchar_t *toRender = lpString;
    if (c >= 0 && c < 4095) {
        wcsncpy(buf, lpString, c);
        buf[c] = L'\0';
        toRender = buf;
    }

    char mbs[4096];
    wcstombs(mbs, toRender, sizeof(mbs));
    mbs[4095] = '\0';
    
    SDL_Surface *surface = TTF_RenderText_Blended(hdc->font, mbs, 0, hdc->textColor);
    if (!surface) return false;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(hdc->renderer, surface);
    SDL_FRect dst = { (float)x, (float)y, (float)surface->w, (float)surface->h };
    SDL_RenderTexture(hdc->renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    return true;
}

bool GetTextExtentPoint32W(HDC hdc, const wchar_t* lpString, int c, SIZE* psiz) {
    if (!hdc || !hdc->font || !lpString || !psiz) return false;

    wchar_t buf[4096];
    const wchar_t *toRender = lpString;
    if (c >= 0 && c < 4095) {
        wcsncpy(buf, lpString, c);
        buf[c] = L'\0';
        toRender = buf;
    }

    char mbs[4096];
    wcstombs(mbs, toRender, sizeof(mbs));
    mbs[4095] = '\0';

    int w, h;
    if (TTF_GetStringSize(hdc->font, mbs, 0, &w, &h)) {
        psiz->cx = w;
        psiz->cy = h;
        return true;
    }
    return false;
}

int DrawTextW(HDC hdc, wchar_t* lpchText, int cchText, RECT* lprc, UINT format) {
    return TextOutW(hdc, lprc->left, lprc->top, lpchText, cchText) ? 1 : 0;
}

int SaveDC(HDC hdc) {
    if (hdc) hdc->savedTarget = hdc->target;
    return 1;
}

bool RestoreDC(HDC hdc, int nSavedDC) {
    if (hdc && hdc->target != hdc->savedTarget) {
        hdc->target = hdc->savedTarget;
        SDL_SetRenderTarget(hdc->renderer, hdc->target);
    }
    return true;
}

bool IntersectClipRect(HDC hdc, int left, int top, int right, int bottom) { return true; }

HBRUSH CreateSolidBrush(COLORREF color) {
    GdiObj *obj = create_gdi_obj(GDI_TYPE_BRUSH, NULL);
    if (obj) obj->color = color;
    return (HBRUSH)obj;
}

void FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr) {
    if (!hdc || !lprc || !hbr) return;
    GdiObj *obj = (GdiObj*)hbr;
    SDL_SetRenderDrawColor(hdc->renderer, GetRValue(obj->color), GetGValue(obj->color), GetBValue(obj->color), 255);
    SDL_FRect r = { (float)lprc->left, (float)lprc->top, (float)(lprc->right - lprc->left), (float)(lprc->bottom - lprc->top) };
    SDL_RenderFillRect(hdc->renderer, &r);
}

int SetStretchBltMode(HDC hdc, int mode) { return 0; }

HPEN CreatePen(int fnPenStyle, int nWidth, COLORREF crColor) {
    GdiObj *obj = create_gdi_obj(GDI_TYPE_PEN, NULL);
    if (obj) obj->color = crColor;
    return (HPEN)obj;
}

HPEN ExtCreatePen(DWORD dwPenStyle, DWORD dwWidth, const LOGBRUSH* lplb, DWORD dwStyleCount, const DWORD* lpStyle) {
    return CreatePen(0, (int)dwWidth, lplb->lbColor);
}

bool Polyline(HDC hdc, const POINT* lppt, int cPoints) {
    if (!hdc || !lppt || cPoints < 2) return false;
    SDL_SetRenderDrawColor(hdc->renderer, hdc->penColor.r, hdc->penColor.g, hdc->penColor.b, 255);
    SDL_FPoint *points = (SDL_FPoint*)malloc(sizeof(SDL_FPoint) * cPoints);
    for (int i = 0; i < cPoints; ++i) {
        points[i].x = (float)lppt[i].x;
        points[i].y = (float)lppt[i].y;
    }
    SDL_RenderLines(hdc->renderer, points, cPoints);
    free(points);
    return true;
}

HDC GetDC(HWND hwnd) {
    static struct HDC_REC screenHdc = {0};
    return &screenHdc;
}
int ReleaseDC(HWND hwnd, HDC hdc) { return 1; }

bool MoveToEx(HDC hdc, int x, int y, POINT* lppt) {
    if (!hdc) return false;
    if (lppt) *lppt = hdc->currentPos;
    hdc->currentPos.x = x;
    hdc->currentPos.y = y;
    return true;
}

bool LineTo(HDC hdc, int x, int y) {
    if (!hdc) return false;
    SDL_SetRenderDrawColor(hdc->renderer, hdc->penColor.r, hdc->penColor.g, hdc->penColor.b, 255);
    SDL_RenderLine(hdc->renderer, (float)hdc->currentPos.x, (float)hdc->currentPos.y, (float)x, (float)y);
    hdc->currentPos.x = x;
    hdc->currentPos.y = y;
    return true;
}

bool SetPixelV(HDC hdc, int x, int y, COLORREF color) {
    if (!hdc) return false;
    SDL_SetRenderDrawColor(hdc->renderer, GetRValue(color), GetGValue(color), GetBValue(color), 255);
    SDL_RenderPoint(hdc->renderer, (float)x, (float)y);
    return true;
}

bool AlphaBlend(HDC hdcDest, int xoriginDest, int yoriginDest, int wdest, int hdest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wsrc, int hsrc, BLENDFUNCTION ftn) {
    if (!hdcDest || !hdcSrc || !hdcSrc->target) return false;
    GdiObj *srcObj = (GdiObj*)hdcSrc->selectedBitmap;
    if (srcObj && srcObj->bits) {
        SDL_UpdateTexture((SDL_Texture*)srcObj->ptr, NULL, srcObj->bits, srcObj->width * 4);
    }
    SDL_FRect src = { (float)xoriginSrc, (float)yoriginSrc, (float)wsrc, (float)hsrc };
    SDL_FRect dst = { (float)xoriginDest, (float)yoriginDest, (float)wdest, (float)hdest };
    SDL_SetTextureAlphaMod(hdcSrc->target, ftn.SourceConstantAlpha);
    SDL_SetTextureBlendMode(hdcSrc->target, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(hdcDest->renderer, hdcSrc->target, &src, &dst);
    return true;
}

HDC CreateCompatibleDC(HDC hdc) {
    if (!hdc) hdc = GetDC(NULL);
    HDC newHdc = (HDC)calloc(1, sizeof(struct HDC_REC));
    if (newHdc) newHdc->renderer = hdc->renderer;
    return newHdc;
}

bool DeleteDC(HDC hdc) {
    if (hdc) free(hdc);
    return true;
}

HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy) {
    SDL_Texture *tex = SDL_CreateTexture(hdc->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (float)cx, (float)cy);
    GdiObj *obj = create_gdi_obj(GDI_TYPE_BITMAP, tex);
    if (obj) {
        obj->width = cx;
        obj->height = cy;
    }
    return (HBITMAP)obj;
}

HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE hSection, DWORD offset) {
    int w = pbmi->bmiHeader.biWidth;
    int h = abs(pbmi->bmiHeader.biHeight);
    SDL_Texture *tex = SDL_CreateTexture(hdc->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (float)w, (float)h);
    if (!tex) return NULL;
    GdiObj *obj = create_gdi_obj(GDI_TYPE_BITMAP, tex);
    if (obj) {
        obj->width = w;
        obj->height = h;
        obj->bits = calloc(1, (size_t)w * (size_t)h * 4);
        if (ppvBits) *ppvBits = obj->bits;
    }
    return (HBITMAP)obj;
}

void* GetStockObject(int fnObject) {
    static GdiObj whiteBrush = { .type = GDI_TYPE_BRUSH, .ptr = NULL, .bits = NULL, .width = 0, .height = 0, .color = 0xFFFFFFFF, .isStock = true };
    static GdiObj nullPen = { .type = GDI_TYPE_PEN, .ptr = NULL, .bits = NULL, .width = 0, .height = 0, .color = 0, .isStock = true };
    if (fnObject == WHITE_BRUSH) return &whiteBrush;
    if (fnObject == NULL_PEN) return &nullPen;
    return NULL;
}

int GetPrivateProfileIntW(const wchar_t* lpAppName, const wchar_t* lpKeyName, int nDefault, const wchar_t* lpFileName) {
    return nDefault;
}

void* ShellExecuteW(HWND hwnd, const wchar_t* lpOperation, const wchar_t* lpFile, const wchar_t* lpParameters, const wchar_t* lpDirectory, int nShowCmd) {
    char url[4096];
    wcstombs(url, lpFile, sizeof(url));
    url[4095] = '\0';
    SDL_OpenURL(url);
    return (void*)33;
}
