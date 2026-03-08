#ifndef CUSTOM_VEHICLE_H
#define CUSTOM_VEHICLE_H

#include "config.h"

void CustomVehicleReset(void);
bool CustomVehicleLoadFromBuilds(const char* buildsPath);
bool CustomVehicleIsLoaded(void);
const char* CustomVehicleName(void);

void DrawCustomVehicleModel(float animAngle);

int CustomVehicleShotpointCount(void);
bool CustomVehicleGetShotpointWorld(int shotpointIndex,
                                    Vector3 vehiclePos,
                                    Matrix vehicleRotation,
                                    Vector3* outWorldPos,
                                    Vector3* outWorldDir,
                                    Color* outColor,
                                    float* outRadius);

#endif
