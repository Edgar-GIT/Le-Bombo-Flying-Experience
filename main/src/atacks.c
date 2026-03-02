#include "atacks.h"


//desenha bomba nuclear em queda
void DrawNukeBombModel(float spinAngle) {
    rlPushMatrix();
        rlRotatef(spinAngle, 0, 1, 0);
        //corpo cilindrico principal
        DrawCylinderEx((Vector3){0, 3.2f, 0}, (Vector3){0, -3.8f, 0},
                       1.05f, 1.35f, 22, (Color){55, 65, 72, 255});
        //cone da frente
        DrawCylinderEx((Vector3){0, 5.3f, 0}, (Vector3){0, 3.2f, 0},
                       0.0f, 1.0f, 22, (Color){75, 88, 98, 255});
        //traseira
        DrawCylinderEx((Vector3){0, -3.8f, 0}, (Vector3){0, -5.0f, 0},
                       1.35f, 0.75f, 20, (Color){45, 52, 58, 255});
        //aletas
        DrawCube((Vector3){ 1.45f,-4.2f, 0.0f}, 0.15f, 2.0f, 1.8f, (Color){95,110,118,255});
        DrawCube((Vector3){-1.45f,-4.2f, 0.0f}, 0.15f, 2.0f, 1.8f, (Color){95,110,118,255});
        DrawCube((Vector3){ 0.0f,-4.2f, 1.45f}, 1.8f, 2.0f, 0.15f, (Color){95,110,118,255});
        DrawCube((Vector3){ 0.0f,-4.2f,-1.45f}, 1.8f, 2.0f, 0.15f, (Color){95,110,118,255});
        //faixa
        DrawCylinderEx((Vector3){0, 0.7f, 0}, (Vector3){0, 0.2f, 0},
                       1.36f, 1.36f, 20, (Color){235, 208, 56, 255});
    rlPopMatrix();
}


//sistema de disparo (blast + metralhadora)
//retorna true quando deve haver tiro neste frame

bool AttackUpdateMachineGun(float dt,
                            float* spaceHeldTime, bool* machineGunActive,
                            float* machineGunFireTimer, float* machineGunCooldown,
                            float* machineAlpha,
                            Sound fxShoot, Sound fxMachine) {
    bool shooting = false;

    //reduz cooldown (so ativo apos metralhadora)
    if (*machineGunCooldown > 0.0f) {
        *machineGunCooldown -= dt;
        if (*machineGunCooldown < 0.0f) *machineGunCooldown = 0.0f;
    }

    if (IsKeyDown(KEY_SPACE)) {
        if (*machineGunCooldown <= 0.0f) {
            *spaceHeldTime += dt;

            if (!*machineGunActive) {
                //blast no primeiro frame
                if (IsKeyPressed(KEY_SPACE)) {
                    PlaySound(fxShoot);
                    shooting = true;
                }

                //apos 1 segundo ativa machine gun
                if (*spaceHeldTime >= MACHINE_GUN_ACTIVATE_TIME) {
                    *machineGunActive = true;
                    *machineGunFireTimer = 0.0f;
                    *machineAlpha = 1.0f;
                    if (!IsSoundPlaying(fxMachine)) PlaySound(fxMachine);
                }
            } else {
                //machine gun dispara continuamente
                *machineGunFireTimer += dt;
                if (*machineGunFireTimer >= MACHINE_GUN_FIRE_RATE) {
                    *machineGunFireTimer = 0.0f;
                    shooting = true;
                    if (!IsSoundPlaying(fxMachine)) PlaySound(fxMachine);
                }
            }
        }
    }

    if (IsKeyReleased(KEY_SPACE)) {
        //cooldown se metralhadora
        if (*machineGunActive) {
            *machineGunCooldown = MACHINE_GUN_COOLDOWN;
        }
        *machineGunActive = false;
        *spaceHeldTime = 0.0f;
        *machineGunFireTimer = 0.0f;
        StopSound(fxMachine);
    }

    return shooting;
}


//regeneraçao automatica de bombas
void AttackUpdateBombInventory(float dt, int* bombCount, float* bombRegenTimer) {
    if (*bombCount < MAX_BOMBS) {
        *bombRegenTimer += dt;
        if (*bombRegenTimer >= BOMB_REGEN_TIME) {
            *bombRegenTimer = 0.0f;
            (*bombCount)++;
        }
    } else {
        *bombRegenTimer = 0.0f;
    }
}


//disparo da bomba normal (B)
void AttackTrySpawnBomb(bool triggerPressed,
                        Bomb* bomb, int* bombCount,
                        VehicleType activeVehicle,
                        Vector3 airplanePos, Matrix rotation, Vector3 forward,
                        Sound fxAlert) {
    if (!triggerPressed || bomb->active || *bombCount <= 0) return;

    bomb->active = true;
    Vector3 bombLocal = { 0.0f, 0.0f, 0.0f };
    if (activeVehicle == VEHICLE_HELICOPTER) {
        bombLocal = (Vector3){ 0.0f, -0.8f, -0.8f };
    } else if (activeVehicle == VEHICLE_JET) {
        bombLocal = (Vector3){ 0.0f, -0.5f, -1.7f };
    }
    bomb->position = Vector3Add(airplanePos, Vector3Transform(bombLocal, rotation));
    bomb->velocity = Vector3Scale(forward, BOMB_FORWARD_SPEED);
    (*bombCount)--;
    PlaySound(fxAlert);
}


//disparo da bomba nuclear (F, 1 time use)
void AttackTrySpawnNuke(bool triggerPressed,
                        NukeBomb* nukeBomb,
                        Vector3 airplanePos, Vector3 forward,
                        float* nukeAlertTimer, float* spFadeAlpha,
                        Sound fxFail) {
    if (!triggerPressed ||
        nukeBomb->used || nukeBomb->active || nukeBomb->waitingImpactSound) {
        return;
    }

    nukeBomb->used     = true;
    nukeBomb->active   = true;
    nukeBomb->position = Vector3Add(airplanePos, Vector3Scale(forward, 100.0f));
    nukeBomb->position.y += 620.0f;
    nukeBomb->velocity = (Vector3){ forward.x * 0.045f, -NUKE_FALL_SPEED, forward.z * 0.045f };
    nukeBomb->spin     = 0.0f;
    *nukeAlertTimer    = 0.0f;
    *spFadeAlpha       = 1.0f;
    PlaySound(fxFail);
}


//fisica da bomba normal + explosao (blast)
void AttackUpdateBomb(float dt,
                      Bomb* bomb, Building* buildings, int* score, float* scaredAlpha,
                      Particle* particles,
                      Color* crazyColors, int numColors,
                      Sound fxKaboom) {
    (void)dt;
    if (!bomb->active) return;

    bomb->position = Vector3Add(bomb->position, bomb->velocity);
    bomb->velocity.y -= BOMB_GRAVITY;

    bool exploded = false;
    if (bomb->position.y <= 1.5f) {
        exploded = true;
        bomb->position.y = 1.5f;
    }

    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].active) {
            BoundingBox bBox = {
                (Vector3){ buildings[i].position.x - buildings[i].size.x/2, 0,
                           buildings[i].position.z - buildings[i].size.z/2 },
                (Vector3){ buildings[i].position.x + buildings[i].size.x/2,
                           buildings[i].size.y,
                           buildings[i].position.z + buildings[i].size.z/2 }
            };
            if (CheckCollisionBoxSphere(bBox, bomb->position, 1.5f)) {
                exploded = true;
                break;
            }
        }
    }

    if (!exploded) return;

    bomb->active = false;
    *scaredAlpha = 1.0f;
    PlaySound(fxKaboom);
    SpawnExplosion(bomb->position, particles, crazyColors, numColors);

    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].active &&
            Vector3Distance(bomb->position, buildings[i].position) <= EXPLOSION_RADIUS) {
            buildings[i].active = false;
            *score += buildings[i].isGolden ? 500 : 100;
        }
    }
}


//blast do laser sobre um edificio
void AttackTryLaserBlastOnBuilding(bool shooting, VehicleType vehicle,
                                   Vector3 airplanePos, Vector3 forward,
                                   Building* building,
                                   int* score, float* laserAlpha,
                                   Particle* particles,
                                   Color* crazyColors, int numColors,
                                   Sound fxExplode) {
    if (!shooting || !building->active) return;

    BoundingBox bBox = {
        (Vector3){ building->position.x - building->size.x/2, 0,
                   building->position.z - building->size.z/2 },
        (Vector3){ building->position.x + building->size.x/2,
                   building->size.y,
                   building->position.z + building->size.z/2 }
    };

    Ray ray = { airplanePos, forward };
    RayCollision laserHit = GetRayCollisionBox(ray, bBox);
    if (!laserHit.hit) return;

    building->active = false;
    building->timeSinceHit = 0.0f;
    *score += building->isGolden ? 500 : 100;
    PlaySound(fxExplode);
    *laserAlpha = 1.0f;
    SpawnExplosion(laserHit.point, particles, crazyColors, numColors);
}


//fisica da bomba nuclear e impacto global
void AttackUpdateNuke(float dt,
                      NukeBomb* nukeBomb, Building* buildings, int* score,
                      NukeTrail* nukeTrails, float* nukeTrailTimer, float* nukeAlertTimer,
                      float* nukeCoverAlpha, bool* worldWipedByNuke,
                      bool* nukeRainActive, float* nukeRainTimer, float* nukeRainSpawnTimer,
                      int* lastKirkScore, int* lastDimaScore,
                      Particle* particles,
                      Sound fxNukeHit) {

    if (nukeBomb->active) {
        *nukeAlertTimer += dt * NUKE_ALERT_BLINK_SPEED;
        nukeBomb->position = Vector3Add(nukeBomb->position, nukeBomb->velocity);
        nukeBomb->velocity.y -= 0.06f * dt;
        nukeBomb->spin += NUKE_ROT_SPEED * dt;

        *nukeTrailTimer += dt;
        while (*nukeTrailTimer >= NUKE_TRAIL_INTERVAL) {
            *nukeTrailTimer -= NUKE_TRAIL_INTERVAL;
            for (int i = 0; i < MAX_NUKE_TRAILS; i++) {
                if (!nukeTrails[i].active) {
                    nukeTrails[i].active = true;
                    nukeTrails[i].position = Vector3Add(nukeBomb->position, (Vector3){
                        (float)GetRandomValue(-180, 180) / 10.0f,
                        (float)GetRandomValue(420, 680) / 10.0f,
                        (float)GetRandomValue(-180, 180) / 10.0f
                    });
                    nukeTrails[i].velocity = (Vector3){
                        (float)GetRandomValue(-14, 14) / 100.0f,
                        (float)GetRandomValue(8, 30) / 100.0f,
                        (float)GetRandomValue(-14, 14) / 100.0f
                    };
                    nukeTrails[i].lifetime = 1.35f;
                    nukeTrails[i].maxLife  = 1.35f;
                    nukeTrails[i].size     = (float)GetRandomValue(45, 130) / 10.0f;
                    break;
                }
            }
        }

        bool nukeHit = false;
        float nukeGroundTouchY = NUKE_MODEL_SCALE * 5.0f;
        if (nukeBomb->position.y <= nukeGroundTouchY) {
            nukeBomb->position.y = nukeGroundTouchY;
            nukeHit = true;
        }

        for (int i = 0; i < MAX_BUILDINGS && !nukeHit; i++) {
            if (buildings[i].active) {
                BoundingBox bBox = {
                    (Vector3){ buildings[i].position.x - buildings[i].size.x/2, 0,
                               buildings[i].position.z - buildings[i].size.z/2 },
                    (Vector3){ buildings[i].position.x + buildings[i].size.x/2,
                               buildings[i].size.y,
                               buildings[i].position.z + buildings[i].size.z/2 }
                };
                if (CheckCollisionBoxSphere(bBox, nukeBomb->position, NUKE_HIT_RADIUS)) {
                    nukeHit = true;
                }
            }
        }

        if (nukeHit) {
            nukeBomb->active = false;
            nukeBomb->waitingImpactSound = true;
            PlaySound(fxNukeHit);
        }
        return;
    }

    if (!nukeBomb->waitingImpactSound) return;

    *nukeAlertTimer += dt * NUKE_ALERT_BLINK_SPEED;
    nukeBomb->spin += (NUKE_ROT_SPEED * 0.25f) * dt;
    if (IsSoundPlaying(fxNukeHit)) return;

    nukeBomb->waitingImpactSound = false;
    *nukeCoverAlpha = 1.0f;
    *worldWipedByNuke = true;
    *nukeRainActive = true;
    *nukeRainTimer = 0.0f;
    *nukeRainSpawnTimer = 0.0f;
    *score += NUKE_SCORE_BONUS;
    *lastKirkScore = *score;
    *lastDimaScore = *score;

    SpawnNukeExplosion(nukeBomb->position, particles);
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].active) {
            buildings[i].active = false;
            buildings[i].timeSinceHit = 0.0f;
        }
    }
}


//chuva de blocos pos-nuke (spawn) e fim do evento
//retorna true quando a chuva terminou

bool AttackUpdateNukeRain(float dt,
                          bool* nukeRainActive,
                          float* nukeRainTimer, float* nukeRainSpawnTimer,
                          RainBlock* rainBlocks,
                          Vector3 airplanePos) {
    if (!*nukeRainActive) return false;

    *nukeRainTimer += dt;
    *nukeRainSpawnTimer += dt;

    while (*nukeRainSpawnTimer >= NUKE_RAIN_SPAWN_INTERVAL) {
        *nukeRainSpawnTimer -= NUKE_RAIN_SPAWN_INTERVAL;
        for (int s = 0; s < 4; s++) {
            for (int i = 0; i < MAX_NUKE_RAIN_BLOCKS; i++) {
                if (!rainBlocks[i].active) {
                    rainBlocks[i].active = true;
                    rainBlocks[i].position = (Vector3){
                        airplanePos.x + (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE),
                        airplanePos.y + (float)GetRandomValue(180, 460),
                        airplanePos.z + (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE)
                    };
                    rainBlocks[i].velocity = (Vector3){
                        (float)GetRandomValue(-30, 30) / 100.0f,
                        -((float)GetRandomValue(140, 310) / 100.0f),
                        (float)GetRandomValue(-30, 30) / 100.0f
                    };
                    rainBlocks[i].size = (Vector3){
                        (float)GetRandomValue(16, 70) / 10.0f,
                        (float)GetRandomValue(10, 44) / 10.0f,
                        (float)GetRandomValue(16, 70) / 10.0f
                    };
                    Color rc[] = {
                        (Color){80, 80, 90, 255},
                        (Color){105, 105, 115, 255},
                        (Color){160, 120, 70, 255},
                        (Color){210, 90, 35, 255}
                    };
                    rainBlocks[i].color = rc[GetRandomValue(0, 3)];
                    break;
                }
            }
        }
    }

    if (*nukeRainTimer < NUKE_RAIN_DURATION) return false;
    *nukeRainActive = false;
    return true;
}


//fisica do rasto da bomba nuclear
void AttackUpdateNukeTrails(float dt, NukeTrail* nukeTrails) {
    for (int i = 0; i < MAX_NUKE_TRAILS; i++) {
        if (nukeTrails[i].active) {
            nukeTrails[i].position = Vector3Add(nukeTrails[i].position, nukeTrails[i].velocity);
            nukeTrails[i].velocity.y += 0.01f;
            nukeTrails[i].lifetime -= dt;
            nukeTrails[i].size += dt * 2.0f;
            if (nukeTrails[i].lifetime <= 0.0f) nukeTrails[i].active = false;
        }
    }
}


//fisica dos blocos da chuva pos-nuke
void AttackUpdateRainBlocks(float dt, RainBlock* rainBlocks) {
    for (int i = 0; i < MAX_NUKE_RAIN_BLOCKS; i++) {
        if (rainBlocks[i].active) {
            rainBlocks[i].position = Vector3Add(rainBlocks[i].position, rainBlocks[i].velocity);
            rainBlocks[i].velocity.y -= 0.035f;
            if (rainBlocks[i].position.y <= 0.0f) {
                rainBlocks[i].active = false;
            }
        }
    }
    (void)dt;
}


//spawna explosao normal
void SpawnExplosion(Vector3 pos, Particle* particles,
                    Color* crazyColors, int numColors) {

    //faiscas coloridas
    int s = 0;
    for (int i = 0; i < MAX_PARTICLES && s < PARTICLE_COUNT_EXPLOSION; i++) {
        if (!particles[i].active) {
            particles[i].active      = true;
            particles[i].position    = pos;
            particles[i].isDebris    = false;
            particles[i].isFireball  = false;
            float speed = (float)GetRandomValue(50, 150) / 10.0f;
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float vert  = (float)GetRandomValue(20, 180) / 10.0f;
            particles[i].velocity = (Vector3){
                cosf(angle) * speed,
                vert,
                sinf(angle) * speed
            };
            particles[i].color       = crazyColors[GetRandomValue(0, numColors-1)];
            particles[i].lifetime    = (float)GetRandomValue(20, 80) / 10.0f;
            particles[i].maxLifetime = particles[i].lifetime;
            particles[i].size        = (float)GetRandomValue(2, 8) / 10.0f;
            s++;
        }
    }

    //bolas de fogo (laranja/amarelo, sobem e dissipam)
    int f = 0;
    for (int i = 0; i < MAX_PARTICLES && f < FIRE_BALL_COUNT; i++) {
        if (!particles[i].active) {
            particles[i].active      = true;
            particles[i].position    = pos;
            particles[i].isDebris    = false;
            particles[i].isFireball  = true;
            particles[i].velocity    = (Vector3){
                (float)GetRandomValue(-30, 30) / 10.0f,
                (float)GetRandomValue(15, 60)  / 10.0f,
                (float)GetRandomValue(-30, 30) / 10.0f
            };
            Color fireColors[] = { ORANGE, YELLOW, RED,
                                   (Color){255,140,0,255},
                                   (Color){255,80,0,255} };
            particles[i].color       = fireColors[GetRandomValue(0, 4)];
            particles[i].lifetime    = (float)GetRandomValue(15, 45) / 10.0f;
            particles[i].maxLifetime = particles[i].lifetime;
            particles[i].size        = (float)GetRandomValue(15, 50) / 10.0f;
            f++;
        }
    }

    //destroços (blocos pesados que caem e rebatem)
    int d = 0;
    for (int i = 0; i < MAX_PARTICLES && d < DEBRIS_COUNT; i++) {
        if (!particles[i].active) {
            particles[i].active      = true;
            particles[i].position    = pos;
            particles[i].isDebris    = true;
            particles[i].isFireball  = false;
            particles[i].velocity    = (Vector3){
                (float)GetRandomValue(-60, 60) / 10.0f,
                (float)GetRandomValue(10, 80)  / 10.0f,
                (float)GetRandomValue(-60, 60) / 10.0f
            };
            Color dc[] = { DARKGRAY, GRAY, BROWN, DARKBROWN,
                           (Color){80,80,80,255}, (Color){60,50,40,255} };
            particles[i].color       = dc[GetRandomValue(0, 5)];
            particles[i].lifetime    = (float)GetRandomValue(60, 180) / 10.0f;
            particles[i].maxLifetime = particles[i].lifetime;
            particles[i].size        = (float)GetRandomValue(8, 35)  / 10.0f;
            d++;
        }
    }
}

//spawna mega explosao nuclear 
void SpawnNukeExplosion(Vector3 pos, Particle* particles) {
    int spawned = 0;
    int targetCount = (int)(MAX_PARTICLES * 0.95f);
    for (int i = 0; i < MAX_PARTICLES && spawned < targetCount; i++) {
        if (!particles[i].active) {
            particles[i].active     = true;
            particles[i].position   = pos;
            particles[i].isDebris   = (GetRandomValue(0, 99) < 30);
            particles[i].isFireball = !particles[i].isDebris;

            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float spread = (float)GetRandomValue(10, 180) / 10.0f;
            float up = (float)GetRandomValue(110, 460) / 10.0f;
            particles[i].velocity = (Vector3){
                cosf(angle) * spread,
                up,
                sinf(angle) * spread
            };

            if (particles[i].isDebris) {
                Color dc[] = { DARKGRAY, GRAY, BROWN, BLACK };
                particles[i].color = dc[GetRandomValue(0, 3)];
                particles[i].size = (float)GetRandomValue(12, 45) / 10.0f;
            } else {
                Color fc[] = { ORANGE, YELLOW, RED, WHITE, (Color){255,120,0,255} };
                particles[i].color = fc[GetRandomValue(0, 4)];
                particles[i].size = (float)GetRandomValue(8, 34) / 10.0f;
            }

            particles[i].lifetime    = (float)GetRandomValue(35, 180) / 10.0f;
            particles[i].maxLifetime = particles[i].lifetime;
            spawned++;
        }
    }
}
