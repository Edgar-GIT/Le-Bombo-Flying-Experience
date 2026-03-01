#ifndef UI_H
#define UI_H

#include "config.h"

// Resultados de interação no menu principal
typedef struct {
    bool leave;
    bool play;
} MainMenuResult;

// Resultados de interação no menu de seleção
typedef struct {
    bool requestPrev;
    bool requestNext;
    bool play;
    bool back;
} VehicleMenuResult;

bool DrawGoofyButton(Rectangle rect, const char* label, Color baseColor, float t);
bool DrawArrowButton(Rectangle rect, bool left, float t);
void DrawGoofyTitle(const char* text, float t);

MainMenuResult UIDrawMainMenu(Texture2D imgMenu, float t);
VehicleMenuResult UIDrawVehicleMenuUI(float t, float uiOffsetY,
                                      VehicleType previewVehicle,
                                      bool menuSliding);
void UIDrawLoseScreen(Texture2D imgLose, bool blinkOn);

void UIDrawGameplayOverlays(Texture2D imgBomb, Texture2D imgScared,
                            Texture2D imgKirk, Texture2D imgDima,
                            Texture2D imgNk, Texture2D imgMachine,
                            Texture2D imgSp,
                            float laserAlpha, float scaredAlpha,
                            float kirkAlpha, float dimaAlpha,
                            float nkAlpha, float machineAlpha,
                            float spFadeAlpha, float nukeCoverAlpha);

void UIDrawGameplayHUD(int score, bool blinkOn,
                       int bombCount, float bombRegenTimer,
                       bool nukeUsed, float machineGunCooldown,
                       bool machineGunActive, float spaceHeldTime,
                       bool nukeActive, bool nukeWaitingImpact,
                       float nukeAlertTimer, bool nukeRainActive);

void UIDrawCrosshair(void);

#endif
