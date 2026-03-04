#include "screens.h"


//gestao de eventos de score + musicas/imagens especiais

//eventos de score: dispara no valor base e depois a cada +100k
static bool TriggerScoreEventEvery100k(int score, int* lastTriggerScore, int firstThreshold) {
    int nextTriggerScore = (*lastTriggerScore <= 0) ? firstThreshold : (*lastTriggerScore + 100000);
    if (score < nextTriggerScore) return false;
    *lastTriggerScore = nextTriggerScore;
    return true;
}

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
                              Music* musicNk) {
    if (score >= *lastNkScore + NK_SCORE_INTERVAL) {
        *lastNkScore += NK_SCORE_INTERVAL;
        *nkAlpha = 1.0f;
        StopMusicStream(*musicBackground);
        StopMusicStream(*musicKirk);
        StopMusicStream(*musicDima);
        StopMusicStream(*musicEp);
        StopMusicStream(*musicChicken);
        SeekMusicStream(*musicNk, 0.0f);
        PlayMusicStream(*musicNk);
    }

    bool nkEventActive = IsMusicStreamPlaying(*musicNk) || (*nkAlpha > 0.0f);
    if (!nkEventActive) {
        if (TriggerScoreEventEvery100k(score, lastKirkScore, KIRK_SCORE_INTERVAL)) {
            *kirkAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicDima);
            StopMusicStream(*musicEp);
            StopMusicStream(*musicChicken);
            SeekMusicStream(*musicKirk, 0.0f);
            PlayMusicStream(*musicKirk);
        }

        if (IsMusicStreamPlaying(*musicKirk) &&
            (GetMusicTimePlayed(*musicKirk) >= KIRK_MAX_PLAY_TIME)) {
            StopMusicStream(*musicKirk);
            *kirkAlpha = 0.0f;
        }

        if (TriggerScoreEventEvery100k(score, lastDimaScore, DIMA_SCORE_INTERVAL)) {
            *dimaAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicKirk);
            StopMusicStream(*musicEp);
            StopMusicStream(*musicChicken);
            SeekMusicStream(*musicDima, 0.0f);
            PlayMusicStream(*musicDima);
        }

        if (TriggerScoreEventEvery100k(score, lastEpScore, EP_SCORE_INTERVAL)) {
            *epAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicKirk);
            StopMusicStream(*musicDima);
            StopMusicStream(*musicChicken);
            SeekMusicStream(*musicEp, 0.0f);
            PlayMusicStream(*musicEp);
        }

        if (TriggerScoreEventEvery100k(score, lastChickenScore, CHICKEN_SCORE_INTERVAL)) {
            *chickenAlpha = 1.0f;
            StopMusicStream(*musicBackground);
            StopMusicStream(*musicKirk);
            StopMusicStream(*musicDima);
            StopMusicStream(*musicEp);
            SeekMusicStream(*musicChicken, 0.0f);
            PlayMusicStream(*musicChicken);
        }
    }

    if (!IsMusicStreamPlaying(*musicKirk) && *kirkAlpha <= 0.0f &&
        !IsMusicStreamPlaying(*musicDima) && *dimaAlpha <= 0.0f &&
        !IsMusicStreamPlaying(*musicEp) && *epAlpha <= 0.0f &&
        !IsMusicStreamPlaying(*musicChicken) && *chickenAlpha <= 0.0f &&
        !IsMusicStreamPlaying(*musicNk) && *nkAlpha <= 0.0f) {
        if (!IsMusicStreamPlaying(*musicBackground)) {
            PlayMusicStream(*musicBackground);
        }
    }
}

//animaçao fade out imagens

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
                        float* nukeCoverAlpha) {
    if (*laserAlpha   > 0.0f) *laserAlpha   -= LASER_FADE_SPEED;
    if (*goldenHitAlpha > 0.0f) *goldenHitAlpha -= LASER_FADE_SPEED;
    if (*scaredAlpha  > 0.0f) *scaredAlpha  -= SCARED_FADE_SPEED;
    if (*kirkAlpha    > 0.0f) *kirkAlpha    -= KIRK_FADE_SPEED;
    if (*dimaAlpha    > 0.0f) *dimaAlpha    -= KIRK_FADE_SPEED;
    if (*epAlpha      > 0.0f) *epAlpha      -= KIRK_FADE_SPEED;
    if (*chickenAlpha > 0.0f) *chickenAlpha -= KIRK_FADE_SPEED;
    if (*nkAlpha      > 0.0f) *nkAlpha      -= NK_FADE_SPEED;
    if (*machineAlpha > 0.0f) *machineAlpha -= MACHINE_IMG_FADE_SPEED;
    if (*spFadeAlpha  > 0.0f) *spFadeAlpha  -= NUKE_SP_FADE_SPEED;
    if (*nukeCoverAlpha > 0.0f) *nukeCoverAlpha -= NUKE_FLASH_FADE_SPEED;
}
