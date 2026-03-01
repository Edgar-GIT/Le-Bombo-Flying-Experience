#ifndef SCREENS_H
#define SCREENS_H

#include "config.h"

void UpdateSpecialScoreEvents(int score,
                              int* lastKirkScore,
                              int* lastDimaScore,
                              int* lastNkScore,
                              float* kirkAlpha,
                              float* dimaAlpha,
                              float* nkAlpha,
                              Music* musicBackground,
                              Music* musicKirk,
                              Music* musicDima,
                              Music* musicNk);

void UpdateOverlayFades(float* laserAlpha,
                        float* scaredAlpha,
                        float* kirkAlpha,
                        float* dimaAlpha,
                        float* nkAlpha,
                        float* machineAlpha,
                        float* spFadeAlpha,
                        float* nukeCoverAlpha);

#endif
