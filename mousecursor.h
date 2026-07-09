#ifndef MOUSECURSOR_H
#define MOUSECURSOR_H

#include "app.h"
#include <stdbool.h>

bool mousecursor_load(const wchar_t *pngPath, int hotspotX, int hotspotY);
void mousecursor_unload(void);
void mousecursor_apply(void);

#endif
