#ifndef OBJ_H
#define OBJ_H

#include "config.h"

const char* GetVehicleName(VehicleType vehicle);
void DrawAirplaneModel(float propellerAngle);
void DrawHelicopterModel(float rotorAngle);
void DrawJetModel(float detailAnimAngle);
void DrawUfoModel(float animAngle);
void DrawVehicleModel(VehicleType vehicle, float spinnerAngle);

#endif
