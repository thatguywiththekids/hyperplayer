#ifndef SPECTRUMANALYZER_H
#define SPECTRUMANALYZER_H

#include "app.h"
#include "renderer.h"

void spectrumanalyzer_update(AppState *app, double dt);
void spectrumanalyzer_draw(AppState *app, HP_DrawContext *ctx);

#endif