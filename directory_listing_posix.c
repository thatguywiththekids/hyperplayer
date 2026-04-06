#include "directory_listing_posix.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <wchar.h>

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

static bool refresh_listing(AppState *app, const wchar_t *path) {
    DirectoryListing *dl = &app->directory;
    char mbsPath[MAX_PATH*4];
    
    // Handle ".." by actually going up if possible
    wchar_t resolvedPath[MAX_PATH];
    wcsncpy(resolvedPath, path, MAX_PATH);
    
    // If the path ends in "/..", try to strip the last two components
    size_t len = wcslen(resolvedPath);
    if (len > 3 && wcscmp(resolvedPath + len - 3, L"/..") == 0) {
        resolvedPath[len - 3] = L'\0'; // Remove "/.."
        wchar_t *lastSlash = wcsrchr(resolvedPath, L'/');
        if (lastSlash) {
            *lastSlash = L'\0'; // Remove the actual last directory component
        }
        if (resolvedPath[0] == L'\0') {
            wcsncpy(resolvedPath, L"/", MAX_PATH);
        }
    }

    wcstombs(mbsPath, resolvedPath, sizeof(mbsPath));
    
    DIR *d = opendir(mbsPath);
    if (!d) return false;
    
    wcsncpy(dl->currentPath, resolvedPath, MAX_PATH);
    dl->scroll = 0; // RESET SCROLL
    
    if (dl->entries) free(dl->entries);
    dl->entries = NULL;
    dl->entryCount = 0;
    dl->entryCapacity = 0;
    
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;
        
        if (dl->entryCount >= dl->entryCapacity) {
            dl->entryCapacity = dl->entryCapacity ? dl->entryCapacity * 2 : 16;
            dl->entries = realloc(dl->entries, sizeof(DirectoryEntry) * dl->entryCapacity);
        }
        
        DirectoryEntry *e = &dl->entries[dl->entryCount++];
        memset(e, 0, sizeof(DirectoryEntry));
        mbstowcs(e->name, de->d_name, 260);
        
        char full[MAX_PATH*4];
        snprintf(full, sizeof(full), "%s/%s", mbsPath, de->d_name);
        
        // Build the full path correctly
        wcsncpy(e->fullPath, resolvedPath, MAX_PATH);
        if (resolvedPath[wcslen(resolvedPath)-1] != L'/') {
            wcsncat(e->fullPath, L"/", MAX_PATH - wcslen(e->fullPath) - 1);
        }
        wcsncat(e->fullPath, e->name, MAX_PATH - wcslen(e->fullPath) - 1);
        
        struct stat st;
        if (stat(full, &st) == 0) {
            e->isDir = S_ISDIR(st.st_mode);
            e->size = (unsigned long long)st.st_size;
        } else {
            e->isDir = (de->d_type == DT_DIR);
            e->size = 0;
        }
        e->isParent = (strcmp(de->d_name, "..") == 0);
    }
    closedir(d);
    
    qsort(dl->entries, dl->entryCount, sizeof(DirectoryEntry), compare_entries);
    return true;
}

bool directory_listing_init(AppState *app, const wchar_t *rootPath) {
    return refresh_listing(app, rootPath);
}

void directory_listing_shutdown(AppState *app) {
    if (app->directory.entries) {
        free(app->directory.entries);
        app->directory.entries = NULL;
    }
}

void directory_listing_draw(AppState *app, HDC hdc) {
    DirectoryListing *dl = &app->directory;
    RECT r = { AREA_X, AREA_Y, AREA_X + AREA_W, AREA_Y + AREA_H };
    HBRUSH br = CreateSolidBrush(RGB(20, 20, 20));
    FillRect(hdc, &r, br);
    DeleteObject(br);
    
    SetTextColor(hdc, RGB(255, 255, 255));
    // Draw current path truncated if needed
    TextOutW(hdc, TEXT_X, AREA_Y + 5, dl->currentPath, (int)wcslen(dl->currentPath));
    
    int visibleRows = AREA_H / ROW_H - 2;
    for (int i = 0; i < visibleRows && (i + dl->scroll) < dl->entryCount; ++i) {
        DirectoryEntry *e = &dl->entries[i + dl->scroll];
        wchar_t line[512];
        swprintf(line, 512, L"%ls %ls", e->isDir ? L"[DIR]" : L"     ", e->name);
        SetTextColor(hdc, e->isDir ? RGB(255, 255, 100) : RGB(200, 200, 200));
        TextOutW(hdc, TEXT_X, AREA_Y + 30 + i * ROW_H, line, (int)wcslen(line));
    }
}

bool directory_listing_mouse_down(AppState *app, int x, int y) {
    if (!app->showFileBrowser) return false;
    
    DirectoryListing *dl = &app->directory;
    if (x < AREA_X || x > AREA_X + AREA_W || y < AREA_Y + 30 || y > AREA_Y + AREA_H) return false;
    
    int index = (y - (AREA_Y + 30)) / ROW_H + dl->scroll;
    if (index >= 0 && index < dl->entryCount) {
        DirectoryEntry *e = &dl->entries[index];
        if (e->isDir) {
            refresh_listing(app, e->fullPath);
        } else {
            player_load_module(app, e->fullPath, e->name);
            player_play(app);
            app->showFileBrowser = false;
        }
        return true;
    }
    return false;
}

void directory_listing_mouse_wheel(AppState *app, int wheelDelta) {
    DirectoryListing *dl = &app->directory;
    dl->scroll -= wheelDelta / 120;
    if (dl->scroll < 0) dl->scroll = 0;
    if (dl->scroll > dl->entryCount - 1) dl->scroll = dl->entryCount - 1;
}

bool directory_listing_move_to_neighbor(AppState *app, int step) {
    DirectoryListing *dl = &app->directory;
    if (!dl || dl->entryCount == 0) return false;
    
    // Find current file in listing
    int currentIndex = -1;
    for (int i = 0; i < dl->entryCount; ++i) {
        if (wcscmp(dl->entries[i].fullPath, app->currentSelectedFile) == 0) {
            currentIndex = i;
            break;
        }
    }
    
    int nextIndex = currentIndex + step;
    // Skip directories
    while (nextIndex >= 0 && nextIndex < dl->entryCount && dl->entries[nextIndex].isDir) {
        nextIndex += (step > 0 ? 1 : -1);
    }
    
    if (nextIndex >= 0 && nextIndex < dl->entryCount) {
        DirectoryEntry *e = &dl->entries[nextIndex];
        wcsncpy(app->currentSelectedFile, e->fullPath, MAX_PATH);
        wcsncpy(app->currentSelectedName, e->name, 260);
        player_load_module(app, e->fullPath, e->name);
        return true;
    }
    
    return false;
}
