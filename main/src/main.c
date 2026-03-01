#include "config.h"
#include "game.h"

// =====================================================================
// MAIN: ponto de entrada mínimo
// - cria janela/audio
// - executa loop de jogo encapsulado em GameRun()
// - fecha sistema no final
// =====================================================================
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
