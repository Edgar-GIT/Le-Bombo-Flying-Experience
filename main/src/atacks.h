#ifndef ATACKS_H
#define ATACKS_H

#include "config.h"

void DrawNukeBombModel(float spinAngle);
void AttackSetOverlaySuppression(bool suppress);
void AttackFlushDeferredFx(Particle* particles, Color* crazyColors, int numColors, int maxFxPerFrame);
void AttackClearDeferredFx(void);

bool AttackUpdateMachineGun(float dt,
                            float* spaceHeldTime, bool* machineGunActive,
                            float* machineGunFireTimer, float* machineGunCooldown,
                            float* machineAlpha,
                            Sound fxShoot, Sound fxMachine);

void AttackUpdateBombInventory(float dt, int* bombCount, float* bombRegenTimer);

void AttackTrySpawnBomb(bool triggerPressed,
                        Bomb* bomb, int* bombCount,
                        VehicleType activeVehicle,
                        Vector3 airplanePos, Matrix rotation, Vector3 forward,
                        Sound fxAlert);

void AttackTrySpawnNuke(bool triggerPressed,
                        NukeBomb* nukeBomb,
                        Vector3 airplanePos, Vector3 forward,
                        float* nukeAlertTimer, float* spFadeAlpha,
                        Sound fxFail);

bool AttackUpdateBomb(float dt,
                      Bomb* bomb, Building* buildings, int* score, float* scaredAlpha,
                      Particle* particles,
                      Color* crazyColors, int numColors,
                      int* comboHitStreak, int* comboLevel, float* comboTimer,
                      bool triggerBlastFeedback,
                      Sound fxKaboom);

void AttackTryLaserBlastOnBuilding(bool shooting, VehicleType vehicle,
                                   Vector3 airplanePos, Vector3 forward,
                                   Building* building,
                                   int* score,
                                   int* comboHitStreak, int* comboLevel, float* comboTimer,
                                   float* laserAlpha, float* goldenHitAlpha,
                                   Particle* particles,
                                   Color* crazyColors, int numColors,
                                   Sound fxExplode, Sound fxGoldenHit);

void AttackUpdateNuke(float dt,
                      NukeBomb* nukeBomb, Building* buildings, int* score,
                      int* comboHitStreak, int* comboLevel, float* comboTimer,
                      NukeTrail* nukeTrails, float* nukeTrailTimer, float* nukeAlertTimer,
                      float* nukeCoverAlpha, bool* worldWipedByNuke,
                      bool* nukeRainActive, float* nukeRainTimer, float* nukeRainSpawnTimer,
                      int* lastKirkScore, int* lastDimaScore,
                      Particle* particles,
                      Sound fxNukeHit);

bool AttackUpdateNukeRain(float dt,
                          bool* nukeRainActive,
                          float* nukeRainTimer, float* nukeRainSpawnTimer,
                          RainBlock* rainBlocks,
                          Vector3 airplanePos);

void AttackUpdateNukeTrails(float dt, NukeTrail* nukeTrails);
void AttackUpdateRainBlocks(float dt, RainBlock* rainBlocks);

void SpawnExplosion(Vector3 pos, Particle* particles,
                    Color* crazyColors, int numColors);
void SpawnNukeExplosion(Vector3 pos, Particle* particles);

#endif
