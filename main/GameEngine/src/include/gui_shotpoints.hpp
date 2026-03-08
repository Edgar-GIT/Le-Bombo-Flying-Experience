#ifndef GAMEENGINE_GUI_SHOTPOINTS_HPP
#define GAMEENGINE_GUI_SHOTPOINTS_HPP

#include "config.hpp"

#include "raylib.h"

void DrawShotpointsTab(Rectangle panelRect, EditorGuiState &st);
void DrawShotpointPlacementWindow(EditorGuiState &st, float dt);

#endif
