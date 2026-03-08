#ifndef GAMEENGINE_WORKSPACE_TABS_HPP
#define GAMEENGINE_WORKSPACE_TABS_HPP

#include "config.hpp"

void InitWorkspaceTabs(EditorGuiState &st);
void SyncActiveWorkspace(EditorGuiState &st);
bool SwitchWorkspace(EditorGuiState &st, int index);
void ResetWorkspacesForProject(EditorGuiState &st);
int FindWorkspaceByPath(const EditorGuiState &st, const std::string &filePath);
int AddWorkspace(EditorGuiState &st, WorkspaceKind kind, const std::string &name, const std::string &filePath, const EditorSnapshot &snapshot);
bool CloseWorkspace(EditorGuiState &st, int index);
bool RenameWorkspace(EditorGuiState &st, int index, const std::string &name);
int DuplicateWorkspace(EditorGuiState &st, int index, const std::string &name);

#endif
