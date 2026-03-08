#ifndef GAMEENGINE_PIECE_LIBRARY_HPP
#define GAMEENGINE_PIECE_LIBRARY_HPP

#include "config.hpp"

void InitPieceLibrary(EditorGuiState &st);
void ReloadPieceLibrary(EditorGuiState &st);
bool CreatePieceFromSelection(EditorGuiState &st);
bool SaveActivePieceWorkspace(EditorGuiState &st);
bool SpawnPieceTemplateAt(EditorGuiState &st, int pieceIndex, Vector3 spawnPos, bool selectPlaced);

#endif
