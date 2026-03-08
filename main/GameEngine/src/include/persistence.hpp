#ifndef GAMEENGINE_PERSISTENCE_HPP
#define GAMEENGINE_PERSISTENCE_HPP

#include "config.hpp"

void InitProjectPersistence(EditorGuiState &st);
void ShutdownProjectPersistence(EditorGuiState &st);
void OpenNewProjectDialog(EditorGuiState &st);
void OpenExistingProjectDialog(EditorGuiState &st);
void DrawProjectDialog(EditorGuiState &st);
bool SaveCurrentProject(EditorGuiState &st, bool autosave);
void UpdateProjectAutosave(EditorGuiState &st);
void MarkProjectDirty(EditorGuiState &st);

#endif
