#ifndef OBJ_H
#define OBJ_H

#include "config.h"

const char* GetVehicleName(VehicleType vehicle);
void DrawAirplaneModel(float propellerAngle);
void DrawHelicopterModel(float rotorAngle);
void DrawJetModel(float detailAnimAngle);
void DrawVehicleModel(VehicleType vehicle, float spinnerAngle);

#endif
