#ifndef SCREENS_H
#define SCREENS_H

#include "config.h"

void UpdateSpecialScoreEvents(int score,
                              int* lastKirkScore,
                              int* lastDimaScore,
                              int* lastEpScore,
                              int* lastChickenScore,
                              int* lastNkScore,
                              float* kirkAlpha,
                              float* dimaAlpha,
                              float* epAlpha,
                              float* chickenAlpha,
                              float* nkAlpha,
                              Music* musicBackground,
                              Music* musicKirk,
                              Music* musicDima,
                              Music* musicEp,
                              Music* musicChicken,
                              Music* musicNk);

void UpdateOverlayFades(float* laserAlpha,
                        float* goldenHitAlpha,
                        float* scaredAlpha,
                        float* kirkAlpha,
                        float* dimaAlpha,
                        float* epAlpha,
                        float* chickenAlpha,
                        float* nkAlpha,
                        float* machineAlpha,
                        float* spFadeAlpha,
                        float* nukeCoverAlpha);

#endif
