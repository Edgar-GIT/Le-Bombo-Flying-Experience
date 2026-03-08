#ifndef GAMEENGINE_PERSISTENCE_PICKER_HPP
#define GAMEENGINE_PERSISTENCE_PICKER_HPP

#include <string>

std::string PickFolderPathNative();
std::string PickFilePathNative(const char *title, const char *pattern);

#endif
