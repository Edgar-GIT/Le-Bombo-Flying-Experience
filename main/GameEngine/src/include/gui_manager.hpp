#ifndef GAMEENGINE_GUI_MANAGER_HPP
#define GAMEENGINE_GUI_MANAGER_HPP

#include "config.hpp"

#include "raylib.h"

#include <cstddef>

extern EditorGuiState gState;
extern const char *gHoverTooltip;
extern bool gBlockUiInput;

float Clampf(float v, float minV, float maxV);
float LerpF(float a, float b, float t);
void SetHoverTooltip(bool hover, const char *text);
void AddLog(EditorGuiState &st, const char *message);
void RecordCacheAction(EditorGuiState &st, const char *action, const char *detail);
void FlushSessionCache(EditorGuiState &st);
void UpdateTextFieldBuffer(char *buffer, size_t bufferSize, bool *submit, bool *cancel);
const PrimitiveDef *FindPrimitiveDef(PrimitiveKind kind);
bool PrimitivePassesFilter(const PrimitiveDef &def, FilterMode mode);
bool DrawButton(Rectangle rect, const char *label, const char *tooltip, bool active = false, int fontSize = 17);
float DistancePointToSegment(Vector2 p, Vector2 a, Vector2 b);
Vector3 ColorToHSVf(Color c);
bool UndoLastAction(EditorGuiState &st);
bool RedoLastAction(EditorGuiState &st);
void ResetHistoryBaseline(EditorGuiState &st);

void DrawEngineGuiLayout(float dt);
void ShutdownEngineGuiLayout();

#endif
