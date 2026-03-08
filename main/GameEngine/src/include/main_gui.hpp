#ifndef GAMEENGINE_MAIN_GUI_HPP
#define GAMEENGINE_MAIN_GUI_HPP

#include "config.hpp"

#include "raylib.h"

void DrawMainGuiLayout(int sw, int sh,
                       float topH, float subTopH,
                       float bodyTop, float statusY, float statusH,
                       const Rectangle &leftIcons,
                       const Rectangle &leftPanel,
                       const Rectangle &viewport,
                       const Rectangle &rightPanel,
                       const Rectangle &statusBar);

#endif
