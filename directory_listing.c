#include "directory_listing.h"
#include "player.h"
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <wchar.h>
#include <wctype.h>

static const int AREA_X = 1363;
static const int AREA_Y = 134;
static const int AREA_W = 555;
static const int AREA_H = 325;
static const int ROW_H = 20;
static const int TEXT_X = 1367;

static int compare_entries(const void *a, const void *b) {
    DirectoryEntry *ea = (DirectoryEntry *)a;
    DirectoryEntry *eb = (DirectoryEntry *)b;
    if (ea->isDir != eb->isDir) return eb->isDir - ea->isDir;
    return wcscmp(ea->name, eb->name);
}

static bool should_keep_file(const wchar_t *name) {
    size_t len = wcslen(name);
    if (len < 4) return false;

    // Check "mod." prefix (case insensitive)
    if (towlower(name[0]) == L'm' &&
        towlower(name[1]) == L'o' &&
        towlower(name[2]) == L'd' &&
        name[3] == L'.') {
        return true;
    }

    // Check ".mod" suffix (case insensitive)
    if (name[len - 4] == L'.' &&
        towlower(name[len - 3]) == L'm' &&
        towlower(name[len - 2]) == L'o' &&
        towlower(name[len - 1]) == L'd') {
        return true;
    }

    return false;
}

#ifdef _WIN32
static bool refresh_listing(AppState *app, const wchar_t *path) {
    DirectoryListing *dl = &app->directory;
    wchar_t resolvedPath[MAX_PATH];
    wcsncpy(resolvedPath, path, MAX_PATH);
    
    size_t len = wcslen(resolvedPath);
    if (len > 3 && wcscmp(resolvedPath + len - 3, L"/..") == 0) {
        resolvedPath[len - 3] = L'\0';
        wchar_t *lastSlash = wcsrchr(resolvedPath, L'/');
        if (!lastSlash) lastSlash = wcsrchr(resolvedPath, L'\\');
        if (lastSlash) *lastSlash = L'\0';
        if (resolvedPath[0] == L'\0') wcsncpy(resolvedPath, L"C:\\", MAX_PATH);
    } else if (len > 3 && wcscmp(resolvedPath + len - 3, L"\\..") == 0) {
        resolvedPath[len - 3] = L'\0';
        wchar_t *lastSlash = wcsrchr(resolvedPath, L'/');
        if (!lastSlash) lastSlash = wcsrchr(resolvedPath, L'\\');
        if (lastSlash) *lastSlash = L'\0';
        if (resolvedPath[0] == L'\0') wcsncpy(resolvedPath, L"C:\\", MAX_PATH);
    }

    wchar_t searchPattern[MAX_PATH*2];
    wcsncpy(searchPattern, resolvedPath, MAX_PATH);
    size_t rlen = wcslen(searchPattern);
    if (rlen > 0 && searchPattern[rlen - 1] != L'/' && searchPattern[rlen - 1] != L'\\') {
        wcsncat(searchPattern, L"\\*", MAX_PATH * 2 - rlen - 1);
    } else {
        wcsncat(searchPattern, L"*", MAX_PATH * 2 - rlen - 1);
    }

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    wcsncpy(dl->currentPath, resolvedPath, MAX_PATH);
    dl->scroll = 0;
    dl->scrollAccumulator = 0.0f;
    
    if (dl->entries) free(dl->entries);
    dl->entries = NULL;
    dl->entryCount = 0;
    dl->entryCapacity = 0;
    
    do {
        if (wcscmp(fd.cFileName, L".") == 0) continue;

        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool isParent = (wcscmp(fd.cFileName, L"..") == 0);

        // Filter files that are not directories or parent navigations
        if (!isDir && !isParent) {
            if (!should_keep_file(fd.cFileName)) {
                continue;
            }
        }

        if (dl->entryCount >= dl->entryCapacity) {
            dl->entryCapacity = dl->entryCapacity ? dl->entryCapacity * 2 : 16;
            dl->entries = realloc(dl->entries, sizeof(DirectoryEntry) * dl->entryCapacity);
        }
        DirectoryEntry *e = &dl->entries[dl->entryCount++];
        memset(e, 0, sizeof(DirectoryEntry));
        wcsncpy(e->name, fd.cFileName, 260);
        
        wcsncpy(e->fullPath, resolvedPath, MAX_PATH);
        size_t flen = wcslen(e->fullPath);
        if (flen > 0 && e->fullPath[flen - 1] != L'/' && e->fullPath[flen - 1] != L'\\') {
            wcsncat(e->fullPath, L"\\", MAX_PATH - flen - 1);
        }
        wcsncat(e->fullPath, e->name, MAX_PATH - wcslen(e->fullPath) - 1);
        
        e->isDir = isDir;
        e->size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        e->isParent = isParent;
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
    qsort(dl->entries, dl->entryCount, sizeof(DirectoryEntry), compare_entries);
    return true;
}
#else
static bool refresh_listing(AppState *app, const wchar_t *path) {
    DirectoryListing *dl = &app->directory;
    char mbsPath[MAX_PATH*4];
    wchar_t resolvedPath[MAX_PATH];
    wcsncpy(resolvedPath, path, MAX_PATH);
    
    size_t len = wcslen(resolvedPath);
    if (len > 3 && wcscmp(resolvedPath + len - 3, L"/..") == 0) {
        resolvedPath[len - 3] = L'\0';
        wchar_t *lastSlash = wcsrchr(resolvedPath, L'/');
        if (lastSlash) *lastSlash = L'\0';
        if (resolvedPath[0] == L'\0') wcsncpy(resolvedPath, L"/", MAX_PATH);
    }

    wcstombs(mbsPath, resolvedPath, sizeof(mbsPath));
    DIR *d = opendir(mbsPath);
    if (!d) return false;
    
    wcsncpy(dl->currentPath, resolvedPath, MAX_PATH);
    dl->scroll = 0;
    dl->scrollAccumulator = 0.0f;
    
    if (dl->entries) free(dl->entries);
    dl->entries = NULL;
    dl->entryCount = 0;
    dl->entryCapacity = 0;
    
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;

        bool isParent = (strcmp(de->d_name, "..") == 0);
        bool isDir = false;
        unsigned long long size = 0;

        char full[MAX_PATH*4];
        snprintf(full, sizeof(full), "%s/%s", mbsPath, de->d_name);

        struct stat st;
        if (stat(full, &st) == 0) {
            isDir = S_ISDIR(st.st_mode);
            size = (unsigned long long)st.st_size;
        } else {
            isDir = (de->d_type == DT_DIR);
        }

        // Filter files that are not directories or parent navigations
        if (!isDir && !isParent) {
            wchar_t wName[260];
            mbstowcs(wName, de->d_name, 260);
            if (!should_keep_file(wName)) {
                continue;
            }
        }

        if (dl->entryCount >= dl->entryCapacity) {
            dl->entryCapacity = dl->entryCapacity ? dl->entryCapacity * 2 : 16;
            dl->entries = realloc(dl->entries, sizeof(DirectoryEntry) * dl->entryCapacity);
        }
        DirectoryEntry *e = &dl->entries[dl->entryCount++];
        memset(e, 0, sizeof(DirectoryEntry));
        mbstowcs(e->name, de->d_name, 260);
        wcsncpy(e->fullPath, resolvedPath, MAX_PATH);
        if (resolvedPath[wcslen(resolvedPath)-1] != L'/') {
            wcsncat(e->fullPath, L"/", MAX_PATH - wcslen(e->fullPath) - 1);
        }
        wcsncat(e->fullPath, e->name, MAX_PATH - wcslen(e->fullPath) - 1);
        e->isDir = isDir;
        e->size = size;
        e->isParent = isParent;
    }
    closedir(d);
    qsort(dl->entries, dl->entryCount, sizeof(DirectoryEntry), compare_entries);
    return true;
}
#endif

bool directory_listing_init(AppState *app, const wchar_t *rootPath) {
    return refresh_listing(app, rootPath);
}

void directory_listing_shutdown(AppState *app) {
    if (app->directory.entries) { free(app->directory.entries); app->directory.entries = NULL; }
}

void directory_listing_draw(AppState *app, HP_DrawContext *ctx) {
    if (!app || !ctx) return;
    DirectoryListing *dl = &app->directory;
    HP_Rect r = { AREA_X, AREA_Y, AREA_W, AREA_H };
    hp_draw_set_color(ctx, (HP_Color){20, 20, 20, 255});
    hp_draw_fill_rect(ctx, &r);
    
    hp_draw_set_font(ctx, app->fonts.dir);
    hp_draw_set_text_color(ctx, (HP_Color){255, 255, 255, 255});
    hp_draw_text(ctx, TEXT_X, AREA_Y + 5, dl->currentPath);
    
    int visibleRows = AREA_H / ROW_H - 2;
    for (int i = 0; i < visibleRows && (i + dl->scroll) < dl->entryCount; ++i) {
        DirectoryEntry *e = &dl->entries[i + dl->scroll];
        wchar_t line[512];
        swprintf(line, 512, L"%ls %ls", e->isDir ? L"[DIR]" : L"     ", e->name);
        hp_draw_set_text_color(ctx, e->isDir ? (HP_Color){255, 255, 100, 255} : (HP_Color){200, 200, 200, 255});
        hp_draw_text(ctx, TEXT_X, AREA_Y + 30 + i * ROW_H, line);
    }
}

bool directory_listing_mouse_down(AppState *app, int x, int y) {
    if (!app->showFileBrowser) return false;
    DirectoryListing *dl = &app->directory;
    if (x < AREA_X || x > AREA_X + AREA_W || y < AREA_Y + 30 || y > AREA_Y + AREA_H) return false;
    int index = (y - (AREA_Y + 30)) / ROW_H + dl->scroll;
    if (index >= 0 && index < dl->entryCount) {
        DirectoryEntry *e = &dl->entries[index];
        if (e->isDir) refresh_listing(app, e->fullPath);
        else { player_load_module(app, e->fullPath, e->name); player_play(app); app->showFileBrowser = false; }
        return true;
    }
    return false;
}

void directory_listing_mouse_wheel(AppState *app, float y) {
    DirectoryListing *dl = &app->directory;
    dl->scrollAccumulator -= y;

    int intDelta = (int)dl->scrollAccumulator;
    if (intDelta != 0) {
        // We have accumulated enough scrolling to scroll the directory list.
        dl->scroll += intDelta;
        dl->scrollAccumulator -= intDelta;

        if (dl->scroll < 0)  {
            dl->scroll = 0;
            dl->scrollAccumulator = 0.0f;
        }
        if (dl->scroll > dl->entryCount - 1) {
            dl->scroll = dl->entryCount - 1;
            dl->scrollAccumulator = 0.0f;
        }
    }
}

bool directory_listing_move_to_neighbor(AppState *app, int step) {
    DirectoryListing *dl = &app->directory;
    if (!dl || dl->entryCount == 0) return false;
    int currentIndex = -1;
    for (int i = 0; i < dl->entryCount; ++i) {
        if (wcscmp(dl->entries[i].fullPath, app->currentSelectedFile) == 0) { currentIndex = i; break; }
    }
    int nextIndex = currentIndex + step;
    while (nextIndex >= 0 && nextIndex < dl->entryCount && dl->entries[nextIndex].isDir) nextIndex += (step > 0 ? 1 : -1);
    if (nextIndex >= 0 && nextIndex < dl->entryCount) {
        DirectoryEntry *e = &dl->entries[nextIndex];
        wcsncpy(app->currentSelectedFile, e->fullPath, MAX_PATH);
        wcsncpy(app->currentSelectedName, e->name, 260);
        player_load_module(app, e->fullPath, e->name);
        return true;
    }
    return false;
}
