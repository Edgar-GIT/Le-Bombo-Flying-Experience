#ifndef ATACKS_H
#define ATACKS_H

#include "config.h"

void DrawNukeBombModel(float spinAngle);

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

void AttackUpdateBomb(float dt,
                      Bomb* bomb, Building* buildings, int* score, float* scaredAlpha,
                      Particle* particles, Shockwave* shockwave, ExplosionFlash* expFlash,
                      Color* crazyColors, int numColors,
                      Sound fxKaboom);

void AttackTryLaserBlastOnBuilding(bool shooting,
                                   Vector3 airplanePos, Vector3 forward,
                                   Building* building,
                                   int* score, float* laserAlpha,
                                   Particle* particles, Shockwave* shockwave,
                                   ExplosionFlash* expFlash,
                                   Color* crazyColors, int numColors,
                                   Sound fxExplode);

void AttackUpdateNuke(float dt,
                      NukeBomb* nukeBomb, Building* buildings, int* score,
                      NukeTrail* nukeTrails, float* nukeTrailTimer, float* nukeAlertTimer,
                      float* nukeCoverAlpha, bool* worldWipedByNuke,
                      bool* nukeRainActive, float* nukeRainTimer, float* nukeRainSpawnTimer,
                      int* lastKirkScore, int* lastDimaScore,
                      Particle* particles, Shockwave* shockwave, ExplosionFlash* expFlash,
                      Sound fxNukeHit);

bool AttackUpdateNukeRain(float dt,
                          bool* nukeRainActive,
                          float* nukeRainTimer, float* nukeRainSpawnTimer,
                          RainBlock* rainBlocks,
                          Vector3 airplanePos);

void AttackUpdateNukeTrails(float dt, NukeTrail* nukeTrails);
void AttackUpdateRainBlocks(float dt, RainBlock* rainBlocks);

void SpawnExplosion(Vector3 pos, Particle* particles,
                    Shockwave* shockwave, ExplosionFlash* flash,
                    Color* crazyColors, int numColors);
void SpawnNukeExplosion(Vector3 pos, Particle* particles,
                        Shockwave* shockwave, ExplosionFlash* flash);

#endif
