#include "sample_list.h"
#include "player.h"
#include "ui.h"
#include "renderer.h"

#include <stdio.h>
#include <wchar.h>

static const int NAME_X = 809;
static const int NUMBER_X = 758;
static const int VOLUME_X = 1144;
static const int SIZE_X = 1269;
static const int START_Y = 242;
static const int ROW_STEP = 27;
static const int ROW_HEIGHT = 27;

static const HP_Color COLOR_WHITE = {255, 255, 255, 255};
static const HP_Color COLOR_SOFT_WHITE = {213, 213, 213, 255};
static const HP_Color COLOR_SHADOW = {89, 89, 89, 255};

void sample_list_draw(AppState *app, HP_DrawContext *ctx)
{
    if (!app || !ctx) {
        return;
    }

    for (int i = 1; i <= 31; ++i) {
        wchar_t indexText[8] = {0};
        wchar_t nameText[64] = {0};
        wchar_t volText[16] = {0};
        wchar_t sizeText[32] = {0};
        HP_Rect nameRect;
        HP_Rect volRect;
        HP_Rect sizeRect;
        int rowY;
        int volume = 0;
        int size = 0;

        if (player_is_loaded(app)) {
            player_get_sample_info(app, i, nameText, sizeof(nameText) / sizeof(nameText[0]), &volume, &size);
        }

        swprintf(indexText, sizeof(indexText) / sizeof(indexText[0]), L"%02X", i);
        swprintf(volText, sizeof(volText) / sizeof(volText[0]), L"%02d", volume);
        swprintf(sizeText, sizeof(sizeText) / sizeof(sizeText[0]), L"%d", size);

        rowY = START_Y + (i - 1) * ROW_STEP;

        nameRect = (HP_Rect){ NAME_X, rowY, 1138 - NAME_X, ROW_HEIGHT };
        volRect = (HP_Rect){ VOLUME_X, rowY, 56, ROW_HEIGHT };
        sizeRect = (HP_Rect){ SIZE_X, rowY, 80, ROW_HEIGHT };

        ui_draw_shadowed_text(ctx, app->fonts.sampleList, indexText, NUMBER_X, rowY, COLOR_SOFT_WHITE, COLOR_SHADOW, 1, 1, NULL, 0);
        ui_draw_shadowed_text(ctx, app->fonts.sampleList, nameText, NAME_X, rowY, COLOR_WHITE, COLOR_SHADOW, 1, 1, &nameRect, 0x00004000); // DT_END_ELLIPSIS
        ui_draw_shadowed_text(ctx, app->fonts.sampleList, volText, VOLUME_X, rowY, COLOR_SOFT_WHITE, COLOR_SHADOW, 1, 1, &volRect, 0x00000002); // DT_RIGHT
        ui_draw_shadowed_text(ctx, app->fonts.sampleList, sizeText, SIZE_X, rowY, COLOR_SOFT_WHITE, COLOR_SHADOW, 1, 1, &sizeRect, 0x00000002); // DT_RIGHT
    }
}

bool sample_list_mouse_down(AppState *app, int x, int y)
{
    int listX1;
    int listX2;
    int listY1;
    int listY2;
    int rowIndex;
    int volume = 0;
    int size = 0;
    wchar_t sampleName[64];
    wchar_t hexText[8];

    if (!app || !player_is_loaded(app)) {
        return false;
    }

    listX1 = 758;
    listX2 = 1349;
    listY1 = START_Y;
    listY2 = START_Y + ((31 - 1) * ROW_STEP) + ROW_HEIGHT;

    if (x < listX1 || x > listX2 || y < listY1 || y > listY2) {
        return false;
    }

    rowIndex = ((y - START_Y) / ROW_STEP) + 1;
    if (rowIndex < 1 || rowIndex > 31) {
        return false;
    }

    app->selectedSampleIndex = rowIndex;

    player_get_sample_info(app, rowIndex, sampleName, sizeof(sampleName) / sizeof(sampleName[0]), &volume, &size);
    swprintf(hexText, sizeof(hexText) / sizeof(hexText[0]), L"%02X", rowIndex);

    if (size <= 1) {
        app_set_status(app, L"Displaying sample # %ls (empty)", hexText);
    } else {
        app_set_status(app, L"Displaying sample # %ls", hexText);
    }

    return true;
}
