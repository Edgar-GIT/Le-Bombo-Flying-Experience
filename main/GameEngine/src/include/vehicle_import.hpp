#ifndef GAMEENGINE_VEHICLE_IMPORT_HPP
#define GAMEENGINE_VEHICLE_IMPORT_HPP

#include "config.hpp"

#include <string>

bool ImportVehicleFile(EditorGuiState &st, const std::string &filePath);
bool ImportVehicleFromPicker(EditorGuiState &st);

#endif
