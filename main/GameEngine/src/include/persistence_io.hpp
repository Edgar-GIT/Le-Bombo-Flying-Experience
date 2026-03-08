#ifndef GAMEENGINE_PERSISTENCE_IO_HPP
#define GAMEENGINE_PERSISTENCE_IO_HPP

#include "config.hpp"

#include <filesystem>

bool SaveProjectJsonFile(const std::filesystem::path &filePath, const EditorGuiState &st);
bool LoadProjectJsonFile(const std::filesystem::path &filePath, EditorGuiState *st);

#endif
