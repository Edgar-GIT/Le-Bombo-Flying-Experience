#include "config.h"
#include "game.h"


int main(void) {
    InitWindow(800, 600, GAME_WINDOW_TITLE);
    ToggleFullscreen();
    InitAudioDevice();
    SetTargetFPS(60);

    GameRun();

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
