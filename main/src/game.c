#include "game.h"
#include "obj.h"
#include "ui.h"
#include "atacks.h"
#include "screens.h"

bool FootprintsOverlap(Vector3 posA, Vector3 sizeA, Vector3 posB, Vector3 sizeB) {
    float aMinX = posA.x - sizeA.x * 0.5f;
    float aMaxX = posA.x + sizeA.x * 0.5f;
    float aMinZ = posA.z - sizeA.z * 0.5f;
    float aMaxZ = posA.z + sizeA.z * 0.5f;

    float bMinX = posB.x - sizeB.x * 0.5f;
    float bMaxX = posB.x + sizeB.x * 0.5f;
    float bMinZ = posB.z - sizeB.z * 0.5f;
    float bMaxZ = posB.z + sizeB.z * 0.5f;

    // Se só encostarem (borda com borda), não conta como sobreposição
    bool overlapX = (aMinX < bMaxX) && (aMaxX > bMinX);
    bool overlapZ = (aMinZ < bMaxZ) && (aMaxZ > bMinZ);
    return overlapX && overlapZ;
}

// =====================================================================
// FUNÇÃO: Verifica se um edifício sobrepõe qualquer outro ativo
// =====================================================================
bool OverlapsAnyBuilding(Vector3 pos, Vector3 size, Building* buildings, int selfIndex) {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (i == selfIndex) continue;
        if (!buildings[i].active) continue;
        if (FootprintsOverlap(pos, size, buildings[i].position, buildings[i].size)) return true;
    }
    return false;
}

// =====================================================================
// FUNÇÃO: Inicializa uma nuvem
// =====================================================================
void InitCloud(Cloud* c, Color* crazyColors, int numColors) {
    c->position = (Vector3){
        (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE),
        (float)GetRandomValue((int)CLOUD_MIN_Y, (int)CLOUD_MAX_Y),
        (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE)
    };
    c->radius = (float)GetRandomValue(12, 38);
    c->color  = crazyColors[GetRandomValue(0, numColors - 1)];
}

// =====================================================================
// FUNÇÃO: Desenha detalhes de janelas/linhas no edifício
// =====================================================================
void DrawBuildingWindows(const Building* b) {
    float yMin = 0.8f;
    float yMax = b->size.y - 0.8f;
    if (yMax <= yMin) return;

    int floors = (int)(b->size.y / 4.0f);
    if (floors < 4) floors = 4;
    if (floors > 14) floors = 14;

    int colsX = (int)(b->size.x / 1.6f);
    if (colsX < 2) colsX = 2;
    if (colsX > 12) colsX = 12;

    int colsZ = (int)(b->size.z / 1.6f);
    if (colsZ < 2) colsZ = 2;
    if (colsZ > 12) colsZ = 12;

    float xL = b->position.x - b->size.x * 0.5f;
    float xR = b->position.x + b->size.x * 0.5f;
    float zF = b->position.z - b->size.z * 0.5f;
    float zB = b->position.z + b->size.z * 0.5f;

    float yStep = (yMax - yMin) / (float)floors;
    float off = 0.03f;
    Color lineColor = b->isGolden ? Fade((Color){255, 210, 100, 255}, 0.75f)
                                  : Fade((Color){210, 235, 255, 255}, 0.45f);

    // Linhas horizontais de fachada
    for (int f = 0; f <= floors; f++) {
        float y = yMin + yStep * (float)f;
        DrawLine3D((Vector3){ xL + 0.25f, y, zF - off }, (Vector3){ xR - 0.25f, y, zF - off }, lineColor);
        DrawLine3D((Vector3){ xL + 0.25f, y, zB + off }, (Vector3){ xR - 0.25f, y, zB + off }, lineColor);
        DrawLine3D((Vector3){ xL - off, y, zF + 0.25f }, (Vector3){ xL - off, y, zB - 0.25f }, lineColor);
        DrawLine3D((Vector3){ xR + off, y, zF + 0.25f }, (Vector3){ xR + off, y, zB - 0.25f }, lineColor);
    }

    // Linhas verticais nas faces frontal/traseira
    for (int c = 1; c < colsX; c++) {
        float t = (float)c / (float)colsX;
        float x = xL + (xR - xL) * t;
        DrawLine3D((Vector3){ x, yMin, zF - off }, (Vector3){ x, yMax, zF - off }, lineColor);
        DrawLine3D((Vector3){ x, yMin, zB + off }, (Vector3){ x, yMax, zB + off }, lineColor);
    }

    // Linhas verticais nas faces laterais
    for (int c = 1; c < colsZ; c++) {
        float t = (float)c / (float)colsZ;
        float z = zF + (zB - zF) * t;
        DrawLine3D((Vector3){ xL - off, yMin, z }, (Vector3){ xL - off, yMax, z }, lineColor);
        DrawLine3D((Vector3){ xR + off, yMin, z }, (Vector3){ xR + off, yMax, z }, lineColor);
    }
}

// =====================================================================
// FUNÇÃO: Inicializa um edifício
// =====================================================================
void InitBuilding(Building* b, Building* buildings, int selfIndex,
                  Color* crazyColors, int numColors) {
    b->isGolden = (GetRandomValue(0, 99) < GOLDEN_BUILDING_CHANCE);
    float w = (float)GetRandomValue(4, 12);
    float d = (float)GetRandomValue(4, 12);
    float h;
    if (b->isGolden) {
        w = (float)GetRandomValue(14, 24);
        d = (float)GetRandomValue(14, 24);
        h = (float)GetRandomValue((int)GOLDEN_MIN_HEIGHT, (int)GOLDEN_MAX_HEIGHT);
        b->color = GOLD;
    } else {
        h = (float)GetRandomValue(10, 60);
        b->color = crazyColors[GetRandomValue(0, numColors - 1)];
    }

    Vector3 newSize = (Vector3){ w, h, d };
    Vector3 newPos = { 0 };
    bool placed = false;

    for (int attempt = 0; attempt < 120; attempt++) {
        newPos = (Vector3){
            (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE),
            h / 2.0f,
            (float)GetRandomValue(-(int)WORLD_HALF_SIZE, (int)WORLD_HALF_SIZE)
        };
        if (!OverlapsAnyBuilding(newPos, newSize, buildings, selfIndex)) {
            placed = true;
            break;
        }
    }

    // Fallback 1: varre grelha para garantir encaixe sem sobreposição
    if (!placed) {
        for (float gx = -WORLD_HALF_SIZE; gx <= WORLD_HALF_SIZE && !placed; gx += 6.0f) {
            for (float gz = -WORLD_HALF_SIZE; gz <= WORLD_HALF_SIZE; gz += 6.0f) {
                Vector3 candidate = (Vector3){ gx, h / 2.0f, gz };
                if (!OverlapsAnyBuilding(candidate, newSize, buildings, selfIndex)) {
                    newPos = candidate;
                    placed = true;
                    break;
                }
            }
        }
    }

    // Fallback 2: se o mapa estiver muito lotado, reduz largura até caber
    if (!placed) {
        float minFootprint = b->isGolden ? 12.0f : 4.0f;
        while (!placed && (newSize.x > minFootprint || newSize.z > minFootprint)) {
            if (newSize.x > minFootprint) newSize.x -= 0.5f;
            if (newSize.z > minFootprint) newSize.z -= 0.5f;

            for (float gx = -WORLD_HALF_SIZE; gx <= WORLD_HALF_SIZE && !placed; gx += 6.0f) {
                for (float gz = -WORLD_HALF_SIZE; gz <= WORLD_HALF_SIZE; gz += 6.0f) {
                    Vector3 candidate = (Vector3){ gx, h / 2.0f, gz };
                    if (!OverlapsAnyBuilding(candidate, newSize, buildings, selfIndex)) {
                        newPos = candidate;
                        placed = true;
                        break;
                    }
                }
            }
        }
    }

    // Se falhar tudo (extremamente improvável), não ativa para evitar sobreposição
    if (!placed) {
        b->active = false;
        b->size = newSize;
        b->baseHeight = h;
        b->position = (Vector3){ 10000.0f, h / 2.0f, 10000.0f };
        b->timeSinceHit = 0.0f;
        return;
    }

    b->size         = newSize;
    b->baseHeight   = h;
    b->position     = newPos;
    b->active       = true;
    b->timeSinceHit = 0.0f;
}

// =====================================================================
// FUNÇÃO: Spawna explosão completa estilo rocket
// Gera: flash branco, bola de fogo, anel de choque, faíscas, destroços
// =====================================================================
void GameRun(void) {

    Sound fxExplode  = LoadSound(SFX_EXPLODE);
    Sound fxShoot    = LoadSound(SFX_SHOOT);
    Sound fxMachine  = LoadSound(SFX_MACHINE);
    Sound fxAlert    = LoadSound(SFX_ALERT);
    Sound fxKaboom   = LoadSound(SFX_KABOOM);
    Sound fxHa       = LoadSound(SFX_HA);
    Sound fxFail     = LoadSound(SFX_FAIL);
    Sound fxNukeHit  = LoadSound(SFX_NUKE_HIT);

    Music musicBackground = LoadMusicStream(MUS_BACKGROUND);
    Music musicKirk       = LoadMusicStream(MUS_KIRK);
    Music musicDima       = LoadMusicStream(MUS_DIMA);
    Music musicNk         = LoadMusicStream(MUS_NK);
    Music musicMenu       = LoadMusicStream(MUS_MENU);
    musicNk.looping       = false;

    Texture2D imgBomb    = LoadTexture(IMG_BOMB);
    Texture2D imgScared  = LoadTexture(IMG_SCARED);
    Texture2D imgKirk    = LoadTexture(IMG_KIRK);
    Texture2D imgDima    = LoadTexture(IMG_DIMA);
    Texture2D imgNk      = LoadTexture(IMG_NK);
    Texture2D imgLose    = LoadTexture(IMG_LOSE);
    Texture2D imgMachine = LoadTexture(IMG_MACHINE);
    Texture2D imgMenu    = LoadTexture(IMG_MENU);
    Texture2D imgSp      = LoadTexture(IMG_SP);

    SetSoundVolume(fxExplode, 1.0f);
    PlayMusicStream(musicMenu);

    Color* crazyColors = gCrazyColors;
    int numCrazyColors = gNumCrazyColors;

    Building buildings[MAX_BUILDINGS] = { 0 };
    for (int i = 0; i < MAX_BUILDINGS; i++)
        InitBuilding(&buildings[i], buildings, i, crazyColors, numCrazyColors);

    Cloud clouds[MAX_CLOUDS];
    for (int i = 0; i < MAX_CLOUDS; i++) InitCloud(&clouds[i], crazyColors, numCrazyColors);

    Bomb           bomb                     = { 0 };
    Particle       particles[MAX_PARTICLES] = { 0 };
    Smoke          smokeArr[MAX_SMOKE]      = { 0 };
    NukeTrail      nukeTrails[MAX_NUKE_TRAILS] = { 0 };
    RainBlock      rainBlocks[MAX_NUKE_RAIN_BLOCKS] = { 0 };
    Shockwave      shockwave                = { 0 };
    ExplosionFlash expFlash                 = { 0 };
    NukeBomb       nukeBomb                 = { 0 };

    int   score         = 0;
    int   lastKirkScore = 0;
    int   lastDimaScore = 0;
    int   lastNkScore   = 0;

    float laserAlpha    = 0.0f;
    float scaredAlpha   = 0.0f;
    float kirkAlpha     = 0.0f;
    float dimaAlpha     = 0.0f;
    float nkAlpha       = 0.0f;
    float machineAlpha  = 0.0f;  // Opacidade da imagem da metralhadora
    float spFadeAlpha   = 0.0f;  // Fade do overlay sp.png após tecla F
    float nukeCoverAlpha = 0.0f; // Cobertura forte do ecrã após mega explosão
    float nukeTrailTimer = 0.0f;
    float nukeAlertTimer = 0.0f;
    float nukeRainTimer = 0.0f;
    float nukeRainSpawnTimer = 0.0f;
    float menuSlideT    = 1.0f;
    int   menuSlideDir  = 0;
    bool  menuSliding   = false;

    int   bombCount      = MAX_BOMBS;
    float bombRegenTimer = 0.0f;
    float colorCycleTimer = 0.0f;
    bool  gameOver       = false;
    bool  worldWipedByNuke = false;
    bool  nukeRainActive  = false;
    float cloudLayerMinY  = CLOUD_MIN_Y;
    float cloudLayerMaxY  = CLOUD_MAX_Y;
    float blinkTimer     = 0.0f;

    // -------------------------------------------------------------------
    // SISTEMA DE DISPARO — estado interno
    //
    // spaceHeldTime: quanto tempo o espaço está segurado NESTA sessão de press
    // machineGunActive: true = está em modo metralhadora
    // machineGunFireTimer: conta para o próximo tiro automático
    // machineGunCooldown: só ativo APÓS sair da metralhadora (não afeta tiro único)
    // -------------------------------------------------------------------
    float spaceHeldTime      = 0.0f;   // Tempo a segurar espaço
    bool  machineGunActive   = false;  // Está em modo metralhadora?
    float machineGunFireTimer = 0.0f;  // Timer entre tiros automáticos
    float machineGunCooldown  = 0.0f;  // Cooldown pós-metralhadora

    Vector3    airplanePos         = { 0.0f, 40.0f, 0.0f };
    Quaternion airplaneOrientation = QuaternionIdentity();
    float      propellerAngle      = 0.0f;
    float      smokeTimer          = 0.0f;

    ScreenState screenState = SCREEN_MENU_MAIN;
    VehicleType previewVehicle = VEHICLE_AIRPLANE;
    VehicleType nextPreviewVehicle = VEHICLE_AIRPLANE;
    VehicleType activeVehicle = VEHICLE_AIRPLANE;

    Camera3D camera = { 0 };
    camera.up         = (Vector3){ 0, 1, 0 };
    camera.fovy       = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    bool firstPersonMode = false;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_X)) break;

        float dt = GetFrameTime();
        float t  = (float)GetTime();
        blinkTimer += dt * BLINK_SPEED;
        bool blinkOn = sinf(blinkTimer * 3.14159f) > 0.0f;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // =====================================================================
        // MENUS
        // =====================================================================
        if (screenState != SCREEN_GAMEPLAY) {
            UpdateMusicStream(musicMenu);
            if (!IsMusicStreamPlaying(musicMenu)) PlayMusicStream(musicMenu);

            if (menuSliding) {
                menuSlideT += dt * 4.5f;
                if (menuSlideT >= 1.0f) {
                    menuSlideT = 1.0f;
                    menuSliding = false;
                    previewVehicle = nextPreviewVehicle;
                }
            }

            BeginDrawing();
                if (screenState == SCREEN_MENU_MAIN) {
                    MainMenuResult menuAction = UIDrawMainMenu(imgMenu, t);
                    if (menuAction.leave) break;
                    if (menuAction.play) {
                        screenState = SCREEN_MENU_VEHICLE;
                        menuSliding = false;
                        menuSlideT = 1.0f;
                        previewVehicle = VEHICLE_AIRPLANE;
                    }
                } else if (screenState == SCREEN_MENU_VEHICLE) {
                    ClearBackground((Color){8, 11, 17, 255});

                    float uiOffsetY = 24.0f; // ligeiro ajuste para baixo

                    Camera3D menuCamera = { 0 };
                    menuCamera.position = (Vector3){ 0.0f, 6.8f, 28.0f };
                    menuCamera.target = (Vector3){ 0.0f, 1.8f, 0.0f };
                    menuCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
                    menuCamera.fovy = 36.0f;
                    menuCamera.projection = CAMERA_PERSPECTIVE;

                    float slideDist = 17.0f;
                    float ease = menuSlideT * menuSlideT * (3.0f - 2.0f * menuSlideT);
                    float previewSpinner = t * PROPELLER_SPEED;
                    float nextPreviewSpinner = t * PROPELLER_SPEED;
                    if (previewVehicle == VEHICLE_HELICOPTER) previewSpinner = t * HELICOPTER_ROTOR_SPEED;
                    else if (previewVehicle == VEHICLE_JET) previewSpinner = t * (PROPELLER_SPEED * 1.6f);
                    if (nextPreviewVehicle == VEHICLE_HELICOPTER) nextPreviewSpinner = t * HELICOPTER_ROTOR_SPEED;
                    else if (nextPreviewVehicle == VEHICLE_JET) nextPreviewSpinner = t * (PROPELLER_SPEED * 1.6f);

                    BeginMode3D(menuCamera);
                        DrawPlane((Vector3){0,0,0}, (Vector2){220,220}, (Color){26,42,38,255});
                        DrawGrid(40, 3.2f);

                        if (menuSliding) {
                            float currentX = -menuSlideDir * ease * slideDist;
                            float nextX = menuSlideDir * (1.0f - ease) * slideDist;

                            rlPushMatrix();
                                rlTranslatef(currentX, 2.6f, 0);
                                rlRotatef(165.0f + sinf(t * 0.7f) * 8.0f, 0, 1, 0);
                                DrawVehicleModel(previewVehicle, previewSpinner);
                            rlPopMatrix();

                            rlPushMatrix();
                                rlTranslatef(nextX, 2.6f, 0);
                                rlRotatef(165.0f + sinf(t * 0.7f) * 8.0f, 0, 1, 0);
                                DrawVehicleModel(nextPreviewVehicle, nextPreviewSpinner);
                            rlPopMatrix();
                        } else {
                            rlPushMatrix();
                                rlTranslatef(0, 2.6f, 0);
                                rlRotatef(165.0f + sinf(t * 0.7f) * 8.0f, 0, 1, 0);
                                DrawVehicleModel(previewVehicle, previewSpinner);
                            rlPopMatrix();
                        }
                    EndMode3D();

                    VehicleMenuResult menuAction = UIDrawVehicleMenuUI(t, uiOffsetY, previewVehicle, menuSliding);
                    if (!menuSliding && menuAction.requestPrev) {
                        menuSliding = true;
                        menuSlideT = 0.0f;
                        menuSlideDir = -1;
                        nextPreviewVehicle = (VehicleType)((previewVehicle + VEHICLE_COUNT - 1) % VEHICLE_COUNT);
                    } else if (!menuSliding && menuAction.requestNext) {
                        menuSliding = true;
                        menuSlideT = 0.0f;
                        menuSlideDir = 1;
                        nextPreviewVehicle = (VehicleType)((previewVehicle + 1) % VEHICLE_COUNT);
                    }

                    if (menuAction.back) {
                        screenState = SCREEN_MENU_MAIN;
                        menuSliding = false;
                        menuSlideT = 1.0f;
                    }

                    if (menuAction.play) {
                        // Arranca sessão de jogo com veículo escolhido
                        activeVehicle = previewVehicle;
                        score = 0; lastKirkScore = 0; lastDimaScore = 0; lastNkScore = 0;
                        bombCount = MAX_BOMBS; bombRegenTimer = 0.0f;
                        laserAlpha = scaredAlpha = kirkAlpha = dimaAlpha = nkAlpha = machineAlpha = 0.0f;
                        spFadeAlpha = nukeCoverAlpha = 0.0f;
                        nukeTrailTimer = 0.0f; nukeAlertTimer = 0.0f;
                        nukeRainTimer = 0.0f; nukeRainSpawnTimer = 0.0f;
                        bomb.active = false; gameOver = false;
                        worldWipedByNuke = false;
                        nukeRainActive = false;
                        nukeBomb = (NukeBomb){ 0 };
                        shockwave.active = false; expFlash.active = false;
                        spaceHeldTime = 0.0f; machineGunActive = false;
                        machineGunCooldown = 0.0f; machineGunFireTimer = 0.0f;
                        airplanePos = (Vector3){ 0, 40, 0 };
                        airplaneOrientation = QuaternionIdentity();
                        propellerAngle = 0.0f;
                        smokeTimer = 0.0f;
                        firstPersonMode = false;
                        for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
                        for (int i = 0; i < MAX_SMOKE; i++)    smokeArr[i].active   = false;
                        for (int i = 0; i < MAX_NUKE_TRAILS; i++) nukeTrails[i].active = false;
                        for (int i = 0; i < MAX_NUKE_RAIN_BLOCKS; i++) rainBlocks[i].active = false;
                        for (int i = 0; i < MAX_BUILDINGS; i++) buildings[i].active = false;
                        for (int i = 0; i < MAX_BUILDINGS; i++)
                            InitBuilding(&buildings[i], buildings, i, crazyColors, numCrazyColors);
                        for (int i = 0; i < MAX_CLOUDS; i++)
                            InitCloud(&clouds[i], crazyColors, numCrazyColors);
                        cloudLayerMinY = CLOUD_MIN_Y;
                        cloudLayerMaxY = CLOUD_MAX_Y;

                        StopSound(fxFail);
                        StopSound(fxNukeHit);
                        StopMusicStream(musicMenu);
                        StopMusicStream(musicKirk);
                        StopMusicStream(musicDima);
                        StopMusicStream(musicNk);
                        SeekMusicStream(musicBackground, 0.0f);
                        PlayMusicStream(musicBackground);
                        screenState = SCREEN_GAMEPLAY;
                    }
                }
            EndDrawing();
            continue;
        }

        // --- GAME OVER ---
        if (gameOver) {
            BeginDrawing();
                UIDrawLoseScreen(imgLose, blinkOn);
            EndDrawing();
            if (IsKeyPressed(KEY_M)) {
                gameOver = false;
                menuSliding = false;
                menuSlideT = 1.0f;
                previewVehicle = activeVehicle;
                nextPreviewVehicle = previewVehicle;
                nkAlpha = 0.0f;
                spFadeAlpha = nukeCoverAlpha = 0.0f;
                nukeBomb.active = false;
                nukeBomb.waitingImpactSound = false;
                nukeRainActive = false;
                StopSound(fxFail);
                StopSound(fxNukeHit);
                StopMusicStream(musicBackground);
                StopMusicStream(musicKirk);
                StopMusicStream(musicDima);
                StopMusicStream(musicNk);
                SeekMusicStream(musicMenu, 0.0f);
                PlayMusicStream(musicMenu);
                screenState = SCREEN_MENU_MAIN;
            }
            if (IsKeyPressed(KEY_R)) {
                score = 0; lastKirkScore = 0; lastDimaScore = 0; lastNkScore = 0;
                bombCount = MAX_BOMBS; bombRegenTimer = 0.0f;
                laserAlpha = scaredAlpha = kirkAlpha = dimaAlpha = nkAlpha = machineAlpha = 0.0f;
                spFadeAlpha = nukeCoverAlpha = 0.0f;
                nukeTrailTimer = 0.0f; nukeAlertTimer = 0.0f;
                nukeRainTimer = 0.0f; nukeRainSpawnTimer = 0.0f;
                bomb.active = false; gameOver = false;
                worldWipedByNuke = false;
                nukeRainActive = false;
                nukeBomb = (NukeBomb){ 0 };
                shockwave.active = false; expFlash.active = false;
                spaceHeldTime = 0.0f; machineGunActive = false;
                machineGunCooldown = 0.0f; machineGunFireTimer = 0.0f;
                airplanePos = (Vector3){ 0, 40, 0 };
                airplaneOrientation = QuaternionIdentity();
                for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
                for (int i = 0; i < MAX_SMOKE; i++)    smokeArr[i].active   = false;
                for (int i = 0; i < MAX_NUKE_TRAILS; i++) nukeTrails[i].active = false;
                for (int i = 0; i < MAX_NUKE_RAIN_BLOCKS; i++) rainBlocks[i].active = false;
                for (int i = 0; i < MAX_BUILDINGS; i++) buildings[i].active = false;
                for (int i = 0; i < MAX_BUILDINGS; i++)
                    InitBuilding(&buildings[i], buildings, i, crazyColors, numCrazyColors);
                for (int i = 0; i < MAX_CLOUDS; i++)
                    InitCloud(&clouds[i], crazyColors, numCrazyColors);
                cloudLayerMinY = CLOUD_MIN_Y;
                cloudLayerMaxY = CLOUD_MAX_Y;
                StopSound(fxFail);
                StopSound(fxNukeHit);
                StopMusicStream(musicKirk); StopMusicStream(musicDima);
                StopMusicStream(musicNk);
                SeekMusicStream(musicBackground, 0.0f);
                PlayMusicStream(musicBackground);
            }
            continue;
        }

        UpdateMusicStream(musicBackground);
        UpdateMusicStream(musicKirk);
        UpdateMusicStream(musicDima);
        UpdateMusicStream(musicNk);

        if (IsKeyPressed(KEY_M)) {
            menuSliding = false;
            menuSlideT = 1.0f;
            previewVehicle = activeVehicle;
            nextPreviewVehicle = previewVehicle;
            nkAlpha = 0.0f;
            spFadeAlpha = nukeCoverAlpha = 0.0f;
            nukeBomb.active = false;
            nukeBomb.waitingImpactSound = false;
            nukeRainActive = false;
            StopSound(fxFail);
            StopSound(fxNukeHit);
            StopMusicStream(musicBackground);
            StopMusicStream(musicKirk);
            StopMusicStream(musicDima);
            StopMusicStream(musicNk);
            SeekMusicStream(musicMenu, 0.0f);
            PlayMusicStream(musicMenu);
            screenState = SCREEN_MENU_MAIN;
            continue;
        }

        if (IsKeyPressed(KEY_O))   firstPersonMode = !firstPersonMode;

        // --- Controlos avião ---
        float pitchInput = 0.0f;
        float yawInput   = 0.0f;
        float rollInput  = 0.0f;
        if (IsKeyDown(KEY_W)) pitchInput += 2.0f * DEG2RAD;
        if (IsKeyDown(KEY_S)) pitchInput -= 2.0f * DEG2RAD;
        if (IsKeyDown(KEY_A)) yawInput   += 2.0f * DEG2RAD;
        if (IsKeyDown(KEY_D)) yawInput   -= 2.0f * DEG2RAD;
        if (IsKeyDown(KEY_Q)) rollInput  -= 3.0f * DEG2RAD;
        if (IsKeyDown(KEY_E)) rollInput  += 3.0f * DEG2RAD;

        bool slowMode = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool fastMode = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        float currentSpeed = AIRPLANE_SPEED;
        if (slowMode) currentSpeed = AIRPLANE_SPEED_SLOW;
        else if (fastMode) currentSpeed = AIRPLANE_SPEED_FAST;
        float spinnerSpeed = PROPELLER_SPEED;
        if (activeVehicle == VEHICLE_HELICOPTER) spinnerSpeed = HELICOPTER_ROTOR_SPEED;
        if (activeVehicle == VEHICLE_JET) spinnerSpeed = PROPELLER_SPEED * 1.6f;
        propellerAngle += spinnerSpeed;

        // CORRIGIDO: orientação do avião via quaternion (sem acumular Euler)
        if (yawInput != 0.0f) {
            Quaternion yawRot = QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, yawInput);
            airplaneOrientation = QuaternionMultiply(yawRot, airplaneOrientation);
        }
        if (pitchInput != 0.0f) {
            Vector3 rightAxis = Vector3RotateByQuaternion((Vector3){ 1, 0, 0 }, airplaneOrientation);
            Quaternion pitchRot = QuaternionFromAxisAngle(Vector3Normalize(rightAxis), pitchInput);
            airplaneOrientation = QuaternionMultiply(pitchRot, airplaneOrientation);
        }
        if (rollInput != 0.0f) {
            Vector3 forwardAxis = Vector3RotateByQuaternion((Vector3){ 0, 0, -1 }, airplaneOrientation);
            Quaternion rollRot = QuaternionFromAxisAngle(Vector3Normalize(forwardAxis), rollInput);
            airplaneOrientation = QuaternionMultiply(rollRot, airplaneOrientation);
        }
        airplaneOrientation = QuaternionNormalize(airplaneOrientation);

        Matrix rotation = QuaternionToMatrix(airplaneOrientation);
        Vector3 forward = Vector3RotateByQuaternion((Vector3){ 0, 0, -1 }, airplaneOrientation);
        Vector3 right   = Vector3RotateByQuaternion((Vector3){ 1, 0, 0 }, airplaneOrientation);
        Vector3 up      = Vector3RotateByQuaternion((Vector3){ 0, 1, 0 }, airplaneOrientation);
        Vector3 back    = Vector3Scale(forward, -1.0f);

        airplanePos = Vector3Add(airplanePos, Vector3Scale(forward, currentSpeed));
        if (airplanePos.y < AIRPLANE_MIN_Y) {
            airplanePos.y = AIRPLANE_MIN_Y;
        }

        Vector3 cockpitOffset = (Vector3){ 0.0f, 1.5f, 1.0f };
        Vector3 chaseOffset   = (Vector3){ 0.0f, 8.0f, 22.0f };
        if (activeVehicle == VEHICLE_HELICOPTER) {
            cockpitOffset = (Vector3){ 0.0f, 2.2f, 1.8f };
            chaseOffset   = (Vector3){ 0.0f, 10.0f, 24.0f };
        } else if (activeVehicle == VEHICLE_JET) {
            cockpitOffset = (Vector3){ 0.0f, 1.6f, 0.9f };
            chaseOffset   = (Vector3){ 0.0f, 9.2f, 25.0f };
        }

        // Câmara: fica ATRÁS do veículo (+Z local = parte de trás)
        if (firstPersonMode) {
            // Câmara no cockpit (ligeiramente atrás do centro)
            camera.position = Vector3Add(airplanePos,
                Vector3Add(Vector3Scale(up, cockpitOffset.y), Vector3Scale(back, cockpitOffset.z)));
            camera.target = Vector3Add(camera.position, Vector3Scale(forward, 100.0f));
        } else {
            // Câmara atrás e acima — offset +Z local (cauda do avião)
            camera.position = Vector3Add(airplanePos,
                Vector3Add(Vector3Scale(up, chaseOffset.y), Vector3Scale(back, chaseOffset.z)));
            camera.target = Vector3Add(airplanePos, Vector3Scale(forward, 30.0f));
        }
        camera.up = up;

        // --- Fumo ---
        smokeTimer += dt;
        if (smokeTimer >= SMOKE_SPAWN_INTERVAL) {
            smokeTimer = 0.0f;
            for (int i = 0; i < MAX_SMOKE; i++) {
                if (!smokeArr[i].active) {
                    // Fumo sai da cauda (+Z local = atrás)
                    Vector3 tailLocal = (Vector3){ 0.0f, 0.0f, 3.5f };
                    if (activeVehicle == VEHICLE_HELICOPTER) {
                        tailLocal = (Vector3){ 0.0f, 0.4f, 5.2f };
                    } else if (activeVehicle == VEHICLE_JET) {
                        tailLocal = (Vector3){ 0.0f, 0.0f, 6.8f };
                    }
                    Vector3 tail = Vector3Transform(tailLocal, rotation);
                    smokeArr[i].position = Vector3Add(airplanePos, tail);
                    smokeArr[i].lifetime = SMOKE_LIFETIME;
                    smokeArr[i].maxLife  = SMOKE_LIFETIME;
                    smokeArr[i].size     = (float)GetRandomValue(8, 18) / 10.0f;
                    smokeArr[i].active   = true;
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_SMOKE; i++) {
            if (smokeArr[i].active) {
                smokeArr[i].lifetime -= dt;
                smokeArr[i].size     += dt * SMOKE_EXPAND_SPEED;
                if (smokeArr[i].lifetime <= 0) smokeArr[i].active = false;
            }
        }

        // -------------------------------------------------------------------
        // SISTEMA DE DISPARO / METRALHADORA (módulo de ataques)
        // -------------------------------------------------------------------
        bool shooting = AttackUpdateMachineGun(dt,
                                               &spaceHeldTime, &machineGunActive,
                                               &machineGunFireTimer, &machineGunCooldown,
                                               &machineAlpha,
                                               fxShoot, fxMachine);

        // --- Bomba ---
        AttackTrySpawnBomb(IsKeyPressed(KEY_B),
                           &bomb, &bombCount,
                           activeVehicle,
                           airplanePos, rotation, forward,
                           fxAlert);

        // --- Bomba Nuclear (uma única vez) ---
        AttackTrySpawnNuke(IsKeyPressed(KEY_F),
                           &nukeBomb,
                           airplanePos, forward,
                           &nukeAlertTimer, &spFadeAlpha,
                           fxFail);

        AttackUpdateBombInventory(dt, &bombCount, &bombRegenTimer);

        // --- Física bomba + blast ---
        AttackUpdateBomb(dt, &bomb, buildings, &score, &scaredAlpha,
                         particles, &shockwave, &expFlash,
                         crazyColors, numCrazyColors,
                         fxKaboom);

        // --- Física bomba nuclear ---
        AttackUpdateNuke(dt,
                         &nukeBomb, buildings, &score,
                         nukeTrails, &nukeTrailTimer, &nukeAlertTimer,
                         &nukeCoverAlpha, &worldWipedByNuke,
                         &nukeRainActive, &nukeRainTimer, &nukeRainSpawnTimer,
                         &lastKirkScore, &lastDimaScore,
                         particles, &shockwave, &expFlash,
                         fxNukeHit);

        // --- Chuva de blocos pós-nuclear ---
        bool rainEnded = AttackUpdateNukeRain(dt,
                                              &nukeRainActive,
                                              &nukeRainTimer, &nukeRainSpawnTimer,
                                              rainBlocks,
                                              airplanePos);
        if (nukeRainActive) {
            // Enquanto a chuva está ativa, a música NK deve continuar sempre.
            StopMusicStream(musicBackground);
            StopMusicStream(musicKirk);
            StopMusicStream(musicDima);
            if (!IsMusicStreamPlaying(musicNk)) {
                SeekMusicStream(musicNk, 0.0f);
                PlayMusicStream(musicNk);
            }
        }
        if (rainEnded) {
            worldWipedByNuke = false;
            StopMusicStream(musicNk);
            nkAlpha = 0.0f;
            for (int i = 0; i < MAX_BUILDINGS; i++) {
                if (!buildings[i].active) {
                    InitBuilding(&buildings[i], buildings, i, crazyColors, numCrazyColors);
                }
            }
        }

        AttackUpdateRainBlocks(dt, rainBlocks);

        colorCycleTimer += dt * 0.5f;

        // --- Loop nuvens (mundo infinito, tal como os edifícios) ---
        for (int i = 0; i < MAX_CLOUDS; i++) {
            float cdx = airplanePos.x - clouds[i].position.x;
            float cdz = airplanePos.z - clouds[i].position.z;

            if (cdx >  WORLD_HALF_SIZE) clouds[i].position.x += WORLD_HALF_SIZE * 2.0f;
            if (cdx < -WORLD_HALF_SIZE) clouds[i].position.x -= WORLD_HALF_SIZE * 2.0f;
            if (cdz >  WORLD_HALF_SIZE) clouds[i].position.z += WORLD_HALF_SIZE * 2.0f;
            if (cdz < -WORLD_HALF_SIZE) clouds[i].position.z -= WORLD_HALF_SIZE * 2.0f;
        }

        // --- Loop edifícios ---
        float tallestBuildingY = CLOUD_MIN_Y;
        for (int i = 0; i < MAX_BUILDINGS; i++) {
            float dx = airplanePos.x - buildings[i].position.x;
            float dz = airplanePos.z - buildings[i].position.z;

            if (dx >  WORLD_HALF_SIZE) buildings[i].position.x += WORLD_HALF_SIZE * 2.0f;
            if (dx < -WORLD_HALF_SIZE) buildings[i].position.x -= WORLD_HALF_SIZE * 2.0f;
            if (dz >  WORLD_HALF_SIZE) buildings[i].position.z += WORLD_HALF_SIZE * 2.0f;
            if (dz < -WORLD_HALF_SIZE) buildings[i].position.z -= WORLD_HALF_SIZE * 2.0f;

            if (buildings[i].active) {
                if (buildings[i].size.y > tallestBuildingY) {
                    tallestBuildingY = buildings[i].size.y;
                }
                if (!buildings[i].isGolden) {
                    float o = (float)i * 0.4f;
                    unsigned char r  = (unsigned char)(fabsf(sinf(colorCycleTimer+o))          * 255);
                    unsigned char g  = (unsigned char)(fabsf(sinf(colorCycleTimer+o+2.094f))   * 255);
                    unsigned char b2 = (unsigned char)(fabsf(sinf(colorCycleTimer+o+4.189f))   * 255);
                    buildings[i].color = (Color){ r, g, b2, 255 };
                }

                buildings[i].timeSinceHit += dt;
                if (buildings[i].timeSinceHit > BUILDING_GROW_DELAY) {
                    float hGrow = BUILDING_GROW_SPEED * (buildings[i].isGolden ? 1.25f : 1.0f);
                    float wGrow = BUILDING_WIDTH_GROW_SPEED * (buildings[i].isGolden ? 1.2f : 1.0f);
                    float maxW = buildings[i].isGolden ? BUILDING_MAX_WIDTH_GOLDEN : BUILDING_MAX_WIDTH;

                    // Altura cresce sempre (sem limite)
                    buildings[i].size.y    += hGrow;
                    buildings[i].position.y = buildings[i].size.y / 2.0f;

                    // Largura cresce até limite e sem sobrepor
                    Vector3 targetSize = buildings[i].size;
                    if (targetSize.x < maxW) targetSize.x = fminf(targetSize.x + wGrow, maxW);
                    if (targetSize.z < maxW) targetSize.z = fminf(targetSize.z + wGrow, maxW);

                    // Primeiro tenta crescer X e Z juntos
                    if (!OverlapsAnyBuilding(buildings[i].position, targetSize, buildings, i)) {
                        buildings[i].size.x = targetSize.x;
                        buildings[i].size.z = targetSize.z;
                    } else {
                        // Se não der juntos, tenta eixo a eixo para ficar "colado" sem atravessar
                        Vector3 onlyX = buildings[i].size;
                        onlyX.x = targetSize.x;
                        if (!OverlapsAnyBuilding(buildings[i].position, onlyX, buildings, i))
                            buildings[i].size.x = onlyX.x;

                        Vector3 onlyZ = buildings[i].size;
                        onlyZ.z = targetSize.z;
                        if (!OverlapsAnyBuilding(buildings[i].position, onlyZ, buildings, i))
                            buildings[i].size.z = onlyZ.z;
                    }
                }

                BoundingBox bBox = {
                    (Vector3){ buildings[i].position.x - buildings[i].size.x/2, 0,
                               buildings[i].position.z - buildings[i].size.z/2 },
                    (Vector3){ buildings[i].position.x + buildings[i].size.x/2,
                               buildings[i].size.y,
                               buildings[i].position.z + buildings[i].size.z/2 }
                };

                AttackTryLaserBlastOnBuilding(shooting,
                                              airplanePos, forward,
                                              &buildings[i],
                                              &score, &laserAlpha,
                                              particles, &shockwave, &expFlash,
                                              crazyColors, numCrazyColors,
                                              fxExplode);

                if (CheckCollisionBoxSphere(bBox, airplanePos, 3.5f)) {
                    StopMusicStream(musicBackground); StopMusicStream(musicKirk);
                    StopMusicStream(musicDima);
                    StopMusicStream(musicNk);
                    StopSound(fxAlert); StopSound(fxKaboom); StopSound(fxMachine);
                    StopSound(fxFail);  StopSound(fxNukeHit);
                    PlaySound(fxHa);
                    gameOver = true;
                }

            } else {
                if (!worldWipedByNuke &&
                    (dx > 380 || dz > 380 || dx < -380 || dz < -380))
                    InitBuilding(&buildings[i], buildings, i, crazyColors, numCrazyColors);
            }
        }

        // --- Altura dinâmica das nuvens (acompanha crescimento dos edifícios) ---
        // Quando os edifícios ficam muito altos, a camada de nuvens sobe gradualmente.
        float cloudLift = fmaxf(0.0f, tallestBuildingY - 120.0f);
        float targetCloudMinY = CLOUD_MIN_Y + cloudLift;
        float targetCloudMaxY = CLOUD_MAX_Y + cloudLift;
        float cloudFollow = fminf(1.0f, dt * 1.6f);
        cloudLayerMinY += (targetCloudMinY - cloudLayerMinY) * cloudFollow;
        cloudLayerMaxY += (targetCloudMaxY - cloudLayerMaxY) * cloudFollow;

        for (int i = 0; i < MAX_CLOUDS; i++) {
            if (clouds[i].position.y < cloudLayerMinY) {
                float p = fminf(1.0f, dt * 2.2f);
                clouds[i].position.y += (cloudLayerMinY - clouds[i].position.y) * p;
            } else if (clouds[i].position.y > cloudLayerMaxY) {
                float p = fminf(1.0f, dt * 2.2f);
                clouds[i].position.y += (cloudLayerMaxY - clouds[i].position.y) * p;
            }
        }

        // --- Física do rasto da bomba nuclear ---
        AttackUpdateNukeTrails(dt, nukeTrails);

        // --- Física partículas ---
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                particles[i].position = Vector3Add(particles[i].position, particles[i].velocity);

                // Destroços caem mais devagar; faíscas e bolas de fogo caem normal
                float grav = particles[i].isDebris ? 3.5f : 8.0f;
                particles[i].velocity.y -= grav * dt;

                if (particles[i].position.y < 0) {
                    particles[i].position.y  = 0;
                    particles[i].velocity.y *= -0.3f;
                    particles[i].velocity.x *= 0.75f;
                    particles[i].velocity.z *= 0.75f;
                }
                particles[i].lifetime -= dt;
                if (particles[i].lifetime <= 0) particles[i].active = false;
            }
        }

        if (shockwave.active) {
            float p = 1.0f - (shockwave.lifetime / shockwave.maxLifetime);
            shockwave.radius    = shockwave.maxRadius * p;
            shockwave.lifetime -= dt;
            if (shockwave.lifetime <= 0) shockwave.active = false;
        }

        if (expFlash.active) {
            expFlash.lifetime -= dt;
            if (expFlash.lifetime <= 0) expFlash.active = false;
        }

        // --- Eventos especiais (pontuação/imagens/músicas) ---
        UpdateSpecialScoreEvents(score,
                                 &lastKirkScore,
                                 &lastDimaScore,
                                 &lastNkScore,
                                 &kirkAlpha,
                                 &dimaAlpha,
                                 &nkAlpha,
                                 &musicBackground,
                                 &musicKirk,
                                 &musicDima,
                                 &musicNk);

        // --- Fades dos overlays ---
        UpdateOverlayFades(&laserAlpha,
                           &scaredAlpha,
                           &kirkAlpha,
                           &dimaAlpha,
                           &nkAlpha,
                           &machineAlpha,
                           &spFadeAlpha,
                           &nukeCoverAlpha);

        // =====================================================================
        // RENDERIZAÇÃO
        // =====================================================================
        BeginDrawing();
            ClearBackground((Color){ 255, 100, 200, 255 });

            BeginMode3D(camera);

                DrawPlane((Vector3){ airplanePos.x, 0, airplanePos.z },
                          (Vector2){ WORLD_HALF_SIZE*4, WORLD_HALF_SIZE*4 }, DARKGREEN);

                rlPushMatrix();
                    rlTranslatef(airplanePos.x, 0, airplanePos.z);
                    DrawGrid(GRID_COUNT, GRID_CELL_SIZE);
                rlPopMatrix();

                for (int i = 0; i < MAX_CLOUDS; i++)
                    DrawSphere(clouds[i].position, clouds[i].radius,
                               Fade(clouds[i].color, 0.8f));

                for (int i = 0; i < MAX_BUILDINGS; i++) {
                    if (buildings[i].active) {
                        DrawCubeV(buildings[i].position, buildings[i].size,
                                  buildings[i].color);
                        DrawCubeWiresV(buildings[i].position, buildings[i].size,
                                       buildings[i].isGolden ? ORANGE : BLACK);
                        DrawBuildingWindows(&buildings[i]);
                    }
                }

                for (int i = 0; i < MAX_NUKE_RAIN_BLOCKS; i++) {
                    if (rainBlocks[i].active) {
                        DrawCubeV(rainBlocks[i].position, rainBlocks[i].size, rainBlocks[i].color);
                        DrawCubeWiresV(rainBlocks[i].position, rainBlocks[i].size, BLACK);
                    }
                }

                // Partículas de explosão
                for (int i = 0; i < MAX_PARTICLES; i++) {
                    if (particles[i].active) {
                        float lr = particles[i].lifetime / particles[i].maxLifetime;

                        if (particles[i].isFireball) {
                            // Bolas de fogo: esferas que encolhem e ficam transparentes
                            float sz = particles[i].size * lr;
                            if (sz < 0.1f) sz = 0.1f;
                            Color c = particles[i].color;
                            c.a = (unsigned char)(lr * 220);
                            DrawSphere(particles[i].position, sz, c);
                        } else if (particles[i].isDebris) {
                            // Destroços: cubos que mantêm tamanho mas ficam mais escuros
                            DrawCube(particles[i].position,
                                     particles[i].size,
                                     particles[i].size,
                                     particles[i].size,
                                     particles[i].color);
                        } else {
                            // Faíscas: cubos pequenos que encolhem e ficam transparentes
                            float sz = particles[i].size * lr;
                            if (sz < 0.05f) sz = 0.05f;
                            Color c = particles[i].color;
                            c.a = (unsigned char)(lr * 255);
                            DrawCube(particles[i].position, sz, sz, sz, c);
                        }
                    }
                }

                // Flash de impacto — esfera branca + bola de fogo
                if (expFlash.active) {
                    float r = expFlash.lifetime / expFlash.maxLifetime;
                    DrawSphere(expFlash.position, 25.0f * r,  Fade(WHITE,  r));
                    DrawSphere(expFlash.position, 18.0f * r,  Fade(ORANGE, r * 0.95f));
                    DrawSphere(expFlash.position, 10.0f * r,  Fade(YELLOW, r * 0.85f));
                    DrawSphere(expFlash.position,  5.0f * r,  Fade(WHITE,  r * 0.7f));
                }

                // Onda de choque
                if (shockwave.active) {
                    float r = shockwave.lifetime / shockwave.maxLifetime;
                    for (int k = 0; k < 5; k++) {
                        float rk = shockwave.radius - k * 4.0f;
                        if (rk > 0)
                            DrawCircle3D(
                                (Vector3){ shockwave.center.x, 0.5f, shockwave.center.z },
                                rk, (Vector3){ 1,0,0 }, 90.0f,
                                Fade((Color){ 255, 200, 50, 255 }, r * 0.8f)
                            );
                    }
                }

                for (int i = 0; i < MAX_SMOKE; i++) {
                    if (smokeArr[i].active) {
                        float a = smokeArr[i].lifetime / smokeArr[i].maxLife;
                        DrawSphere(smokeArr[i].position, smokeArr[i].size,
                                   Fade((Color){ 180,180,180,255 }, a * 0.6f));
                    }
                }

                // Rasto da bomba nuclear
                for (int i = 0; i < MAX_NUKE_TRAILS; i++) {
                    if (nukeTrails[i].active) {
                        float a = nukeTrails[i].lifetime / nukeTrails[i].maxLife;
                        DrawSphere(nukeTrails[i].position, nukeTrails[i].size,
                                   Fade((Color){ 255, 170, 70, 255 }, a * 0.7f));
                        DrawSphere(nukeTrails[i].position, nukeTrails[i].size * 0.45f,
                                   Fade((Color){ 255, 240, 200, 255 }, a * 0.85f));
                    }
                }

                // Bomba nuclear gigante
                if (nukeBomb.active || nukeBomb.waitingImpactSound) {
                    rlPushMatrix();
                        rlTranslatef(nukeBomb.position.x, nukeBomb.position.y, nukeBomb.position.z);
                        rlRotatef(180.0f, 1.0f, 0.0f, 0.0f);
                        rlScalef(NUKE_MODEL_SCALE, NUKE_MODEL_SCALE, NUKE_MODEL_SCALE);
                        DrawNukeBombModel(nukeBomb.spin);
                    rlPopMatrix();
                }

                if (bomb.active) DrawSphere(bomb.position, 1.5f, DARKGRAY);

                // -----------------------------------------------------------
                // VEÍCULO DO JOGADOR
                // O modelo é desenhado no espaço local com frente para -Z
                // -----------------------------------------------------------
                if (!firstPersonMode) {
                    rlPushMatrix();
                        rlTranslatef(airplanePos.x, airplanePos.y, airplanePos.z);
                        rlMultMatrixf(MatrixToFloat(rotation));
                        DrawVehicleModel(activeVehicle, propellerAngle);

                    rlPopMatrix();
                }

                // Laser sai dos canhões (direção forward = -Z local)
                if (shooting) {
                    Vector3 gunRLocal = (Vector3){ 2.5f, -0.05f, 0.0f };
                    Vector3 gunLLocal = (Vector3){ -2.5f, -0.05f, 0.0f };
                    if (activeVehicle == VEHICLE_HELICOPTER) {
                        gunRLocal = (Vector3){ 2.45f, -0.45f, -0.95f };
                        gunLLocal = (Vector3){ -2.45f, -0.45f, -0.95f };
                    } else if (activeVehicle == VEHICLE_JET) {
                        gunRLocal = (Vector3){ 2.55f, -0.15f, -2.0f };
                        gunLLocal = (Vector3){ -2.55f, -0.15f, -2.0f };
                    }
                    Vector3 wR = Vector3Transform(gunRLocal, rotation);
                    Vector3 wL = Vector3Transform(gunLLocal, rotation);
                    Vector3 lR = Vector3Add(airplanePos, wR);
                    Vector3 lL = Vector3Add(airplanePos, wL);
                    DrawCylinderEx(lR, Vector3Add(lR, Vector3Scale(forward, LASER_LENGTH)),
                                   LASER_RADIUS, LASER_RADIUS, 4, LIME);
                    DrawCylinderEx(lL, Vector3Add(lL, Vector3Scale(forward, LASER_LENGTH)),
                                   LASER_RADIUS, LASER_RADIUS, 4, LIME);
                }

            EndMode3D();

            UIDrawGameplayOverlays(imgBomb, imgScared, imgKirk, imgDima, imgNk,
                                   imgMachine, imgSp,
                                   laserAlpha, scaredAlpha, kirkAlpha, dimaAlpha,
                                   nkAlpha, machineAlpha, spFadeAlpha, nukeCoverAlpha);

            UIDrawGameplayHUD(score, blinkOn,
                              bombCount, bombRegenTimer,
                              nukeBomb.used, machineGunCooldown,
                              machineGunActive, spaceHeldTime,
                              nukeBomb.active, nukeBomb.waitingImpactSound,
                              nukeAlertTimer, nukeRainActive);

            if (firstPersonMode) UIDrawCrosshair();

        EndDrawing();
    }

    UnloadSound(fxExplode); UnloadSound(fxShoot); UnloadSound(fxMachine);
    UnloadSound(fxAlert);   UnloadSound(fxKaboom); UnloadSound(fxHa);
    UnloadSound(fxFail);    UnloadSound(fxNukeHit);
    UnloadMusicStream(musicBackground);
    UnloadMusicStream(musicKirk);
    UnloadMusicStream(musicDima);
    UnloadMusicStream(musicNk);
    UnloadMusicStream(musicMenu);
    UnloadTexture(imgBomb);    UnloadTexture(imgScared);
    UnloadTexture(imgKirk);    UnloadTexture(imgDima);    UnloadTexture(imgNk);
    UnloadTexture(imgLose);    UnloadTexture(imgMachine);
    UnloadTexture(imgMenu);    UnloadTexture(imgSp);
}
