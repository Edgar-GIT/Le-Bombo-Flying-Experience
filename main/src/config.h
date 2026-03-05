#ifndef CONFIG_H
#define CONFIG_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <string.h>

// --- JANELA ---
#define GAME_WINDOW_TITLE "Le bombo flight simulator"

// --- MUNDO ---
#define MAX_BUILDINGS          250
#define MAX_CLOUDS             150
#define WORLD_HALF_SIZE        400.0f
#define GRID_CELL_SIZE           8.0f
#define GRID_COUNT             200
#define CLOUD_MIN_Y            200.0f
#define CLOUD_MAX_Y            300.0f

// --- AVIAO ---
#define AIRPLANE_SPEED           2.5f
#define AIRPLANE_SPEED_SLOW      0.75f
#define AIRPLANE_SPEED_FAST      6.6f
#define AIRPLANE_MIN_Y           2.0f
#define PROPELLER_SPEED         30.0f
#define HELICOPTER_ROTOR_SPEED  95.0f

// --- LASER / METRALHADORA ---
#define LASER_LENGTH           300.0f
#define LASER_RADIUS             0.5f
#define MACHINE_GUN_ACTIVATE_TIME 1.0f
#define MACHINE_GUN_FIRE_RATE     0.08f
#define MACHINE_GUN_COOLDOWN      1.0f

// --- IMAGEM DA METRALHADORA ---
#define MACHINE_IMG_DURATION     1.0f
#define MACHINE_IMG_FADE_SPEED   0.025f

// --- BOMBA ---
#define MAX_BOMBS                3
#define BOMB_REGEN_TIME          7.0f
#define BOMB_FORWARD_SPEED       1.2f
#define BOMB_GRAVITY             0.015f
#define KABOOM_DELAY             1.0f
#define MAX_NUKE_TRAILS         260
#define NUKE_FALL_SPEED          0.85f
#define NUKE_ROT_SPEED          85.0f
#define NUKE_TRAIL_INTERVAL      0.02f
#define NUKE_SP_FADE_SPEED       0.020f
#define NUKE_FLASH_FADE_SPEED    0.010f
#define NUKE_ALERT_BLINK_SPEED   8.0f
#define NUKE_MODEL_SCALE        10.2f
#define NUKE_HIT_RADIUS         15.0f
#define NUKE_SCORE_BONUS    1000000
#define NUKE_RAIN_DURATION      45.0f
#define NUKE_RAIN_SPAWN_INTERVAL 0.03f
#define MAX_NUKE_RAIN_BLOCKS     820
#define KIRK_MAX_PLAY_TIME      12.0f

// --- EXPLOSAO ESTILO ROCKET ---
#define MAX_PARTICLES           2200
#define EXPLOSION_RADIUS        250.0f
#define PARTICLE_COUNT_EXPLOSION 500
#define DEBRIS_COUNT            1150
#define FIRE_BALL_COUNT          60

// --- FUMO ---
#define MAX_SMOKE               80
#define SMOKE_SPAWN_INTERVAL     0.05f
#define SMOKE_LIFETIME           1.8f
#define SMOKE_EXPAND_SPEED       1.5f

// --- EDIFICIOS ---
#define GOLDEN_BUILDING_CHANCE   2
#define GOLDEN_MIN_HEIGHT       70.0f
#define GOLDEN_MAX_HEIGHT      120.0f
#define BUILDING_GROW_DELAY      5.0f
#define BUILDING_GROW_SPEED      0.14f
#define BUILDING_WIDTH_GROW_SPEED 0.08f
#define BUILDING_MAX_WIDTH      28.0f
#define BUILDING_MAX_WIDTH_GOLDEN 36.0f

// --- EVENTOS ESPECIAIS ---
#define KIRK_SCORE_INTERVAL     20000
#define DIMA_SCORE_INTERVAL     40000
#define EP_SCORE_INTERVAL       60000
#define CHICKEN_SCORE_INTERVAL  80000
#define NK_SCORE_INTERVAL    1000000
#define KIRK_FADE_SPEED          0.015f
#define NK_FADE_SPEED            0.012f
#define LASER_FADE_SPEED         0.0220f
#define SCARED_FADE_SPEED        0.01065f

// --- HUD ---
#define BLINK_SPEED              4.0f

// --- RECURSOS ---

#define ASSET_MUSIC_DIR   "music/"
#define ASSET_IMG_DIR     "img/"

#define SFX_EXPLODE     ASSET_MUSIC_DIR "explode.mp3"
#define SFX_SHOOT       ASSET_MUSIC_DIR "peido.mp3"
#define SFX_MACHINE     ASSET_MUSIC_DIR "met.mp3"
#define SFX_ALERT       ASSET_MUSIC_DIR "alert.mp3"
#define SFX_KABOOM      ASSET_MUSIC_DIR "kaboom.mp3"
#define SFX_HA          ASSET_MUSIC_DIR "fail.mp3"
#define SFX_FAIL        ASSET_MUSIC_DIR "ha.mp3"
#define SFX_NUKE_HIT    ASSET_MUSIC_DIR "67.mp3"
#define SFX_GOLDEN_HIT  ASSET_MUSIC_DIR "672.mp3"
#define MUS_BACKGROUND  ASSET_MUSIC_DIR "linga.mp3"
#define MUS_KIRK        ASSET_MUSIC_DIR "kirk.mp3"
#define MUS_DIMA        ASSET_MUSIC_DIR "dima.mp3"
#define MUS_EP          ASSET_MUSIC_DIR "ep.mp3"
#define MUS_CHICKEN     ASSET_MUSIC_DIR "chicken.mp3"
#define MUS_NK          ASSET_MUSIC_DIR "nk.mp3"
#define MUS_MENU        ASSET_MUSIC_DIR "cat.mp3"

#define IMG_BOMB        ASSET_IMG_DIR "bombo.png"
#define IMG_SCARED      ASSET_IMG_DIR "scared.png"
#define IMG_KIRK        ASSET_IMG_DIR "kirk.png"
#define IMG_DIMA        ASSET_IMG_DIR "dima.png"
#define IMG_3P          ASSET_IMG_DIR "3p.png"
#define IMG_CHICKEN     ASSET_IMG_DIR "chicken.png"
#define IMG_NK          ASSET_IMG_DIR "nk.png"
#define IMG_GOLDEN_HIT  ASSET_IMG_DIR "67.png"
#define IMG_LOSE        ASSET_IMG_DIR "lose.png"
#define IMG_MACHINE     ASSET_IMG_DIR "machine.png"
#define IMG_MENU        ASSET_IMG_DIR "menu.png"
#define IMG_SP          ASSET_IMG_DIR "sp.png"

// --- STRUCTS DADOS ---

typedef struct {
    Vector3 position;
    Vector3 size;
    float   baseHeight;
    Color   color;
    bool    active;
    bool    isGolden;
    float   timeSinceHit;
} Building;

typedef struct {
    Vector3 position;
    float   radius;
    Color   color;
} Cloud;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    bool    active;
} Bomb;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Color   color;
    float   lifetime;
    float   maxLifetime;
    float   size;
    bool    active;
    bool    isDebris;
    bool    isFireball;
} Particle;

typedef struct {
    Vector3 position;
    float   lifetime;
    float   maxLife;
    float   size;
    bool    active;
} Smoke;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float   lifetime;
    float   maxLife;
    float   size;
    bool    active;
} NukeTrail;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Vector3 size;
    Color   color;
    bool    active;
} RainBlock;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float   spin;
    bool    active;
    bool    waitingImpactSound;
    bool    used;
} NukeBomb;

typedef enum {
    SCREEN_MENU_MAIN = 0,
    SCREEN_MENU_VEHICLE,
    SCREEN_GAMEPLAY
} ScreenState;

typedef enum {
    VEHICLE_AIRPLANE = 0,
    VEHICLE_HELICOPTER,
    VEHICLE_JET,
    VEHICLE_UFO,
    VEHICLE_DRONE,
    VEHICLE_HAWK,
    VEHICLE_COUNT
} VehicleType;

extern Color gCrazyColors[];
extern const int gNumCrazyColors;

#endif
