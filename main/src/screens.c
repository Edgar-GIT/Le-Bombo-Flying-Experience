#include "screens.h"

// =====================================================================
// FUNÇÃO: Gestão de eventos de score + músicas/imagens especiais
// =====================================================================
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
                              Music* musicNk) {
    if (score >= *lastNkScore + NK_SCORE_INTERVAL) {
        *lastNkScore += NK_SCORE_INTERVAL;
        *nkAlpha = 1.0f;
        StopMusicStream(*musicBackground);
        StopMusicStream(*musicKirk);
        StopMusicStream(*musicDima);
        SeekMusicStream(*musicNk, 0.0f);
        PlayMusicStream(*musicNk);
    }

    bool nkEventActive = IsMusicStreamPlaying(*musicNk) || (*nkAlpha > 0.0f);
    if (!nkEventActive) {
        if (score >= *lastKirkScore + KIRK_SCORE_INTERVAL) {
            *lastKirkScore += KIRK_SCORE_INTERVAL;
            *kirkAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicDima);
            SeekMusicStream(*musicKirk, 0.0f);
            PlayMusicStream(*musicKirk);
        }

        if (IsMusicStreamPlaying(*musicKirk) &&
            (GetMusicTimePlayed(*musicKirk) >= KIRK_MAX_PLAY_TIME)) {
            StopMusicStream(*musicKirk);
            *kirkAlpha = 0.0f;
        }

        if (score >= *lastDimaScore + DIMA_SCORE_INTERVAL) {
            *lastDimaScore += DIMA_SCORE_INTERVAL;
            *dimaAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicKirk);
            SeekMusicStream(*musicDima, 0.0f);
            PlayMusicStream(*musicDima);
        }
    }

    if (!IsMusicStreamPlaying(*musicKirk) && *kirkAlpha <= 0.0f &&
        !IsMusicStreamPlaying(*musicDima) &&
        !IsMusicStreamPlaying(*musicNk) && *nkAlpha <= 0.0f) {
        if (!IsMusicStreamPlaying(*musicBackground)) {
            PlayMusicStream(*musicBackground);
        }
    }
}

// =====================================================================
// FUNÇÃO: Fade-out das imagens de overlay
// =====================================================================
void UpdateOverlayFades(float* laserAlpha,
                        float* scaredAlpha,
                        float* kirkAlpha,
                        float* dimaAlpha,
                        float* nkAlpha,
                        float* machineAlpha,
                        float* spFadeAlpha,
                        float* nukeCoverAlpha) {
    if (*laserAlpha   > 0.0f) *laserAlpha   -= LASER_FADE_SPEED;
    if (*scaredAlpha  > 0.0f) *scaredAlpha  -= SCARED_FADE_SPEED;
    if (*kirkAlpha    > 0.0f) *kirkAlpha    -= KIRK_FADE_SPEED;
    if (*dimaAlpha    > 0.0f) *dimaAlpha    -= KIRK_FADE_SPEED;
    if (*nkAlpha      > 0.0f) *nkAlpha      -= NK_FADE_SPEED;
    if (*machineAlpha > 0.0f) *machineAlpha -= MACHINE_IMG_FADE_SPEED;
    if (*spFadeAlpha  > 0.0f) *spFadeAlpha  -= NUKE_SP_FADE_SPEED;
    if (*nukeCoverAlpha > 0.0f) *nukeCoverAlpha -= NUKE_FLASH_FADE_SPEED;
}
