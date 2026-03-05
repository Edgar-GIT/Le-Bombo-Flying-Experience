#include "ui.h"
#include "obj.h"

//desenha goofy buttons pros menus

bool DrawGoofyButton(Rectangle rect, const char* label, Color baseColor, float t) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    float pulse = hover ? (sinf(t * 8.0f) * 3.0f + 4.0f) : 0.0f;
    Rectangle r = {
        rect.x - pulse * 0.5f,
        rect.y - pulse * 0.5f,
        rect.width + pulse,
        rect.height + pulse
    };

    Color fill = hover ? Fade(baseColor, 0.95f) : Fade(baseColor, 0.82f);
    Color border = hover ? YELLOW : RAYWHITE;

    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, hover ? 4.0f : 2.0f, border);

    int fontSize = hover ? 34 : 30;
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x + r.width * 0.5f - tw * 0.5f),
             (int)(r.y + r.height * 0.5f - fontSize * 0.5f), fontSize, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

//desenha botoes de seta lateral

bool DrawArrowButton(Rectangle rect, bool left, float t) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    Color fill = hover ? Fade((Color){ 18, 18, 24, 255 }, 0.98f)
                       : Fade((Color){ 10, 10, 14, 255 }, 0.86f);
    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, hover ? 4.0f : 3.0f, hover ? YELLOW : RAYWHITE);

    float wobble = sinf(t * 9.0f) * (hover ? 5.0f : 2.0f);
    Vector2 c = { rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f };
    float dir = left ? -1.0f : 1.0f;

    Vector2 tip   = { c.x + dir * 18.0f + wobble, c.y };
    Vector2 upper = { c.x - dir * 14.0f + wobble, c.y - 24.0f };
    Vector2 lower = { c.x - dir * 14.0f + wobble, c.y + 24.0f };
    DrawTriangle(upper, tip, lower, WHITE);
    DrawTriangle(lower, tip, upper, WHITE);

    Vector2 tipIn   = { c.x + dir * 11.0f + wobble, c.y };
    Vector2 upperIn = { c.x - dir * 8.0f + wobble, c.y - 14.0f };
    Vector2 lowerIn = { c.x - dir * 8.0f + wobble, c.y + 14.0f };
    DrawTriangle(upperIn, tipIn, lowerIn, Fade((Color){10,10,14,255}, hover ? 0.75f : 0.65f));
    DrawTriangle(lowerIn, tipIn, upperIn, Fade((Color){10,10,14,255}, hover ? 0.75f : 0.65f));

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

//desenhar goofy title

void DrawGoofyTitle(const char* text, float t) {
    Font font = GetFontDefault();
    float fontSize = 62.0f;
    float spacing = 2.0f;
    float totalW = 0.0f;

    for (int i = 0; text[i] != '\0'; i++) {
        char c[2] = { text[i], '\0' };
        totalW += MeasureTextEx(font, c, fontSize, spacing).x;
    }

    float x = GetScreenWidth() * 0.5f - totalW * 0.5f;
    for (int i = 0; text[i] != '\0'; i++) {
        char c[2] = { text[i], '\0' };
        float y = 24.0f + sinf(t * 4.8f + i * 0.45f) * 11.0f;
        float rot = sinf(t * 3.5f + i * 0.3f) * 10.0f;
        Color col = ColorFromHSV(fmodf(t * 130.0f + i * 22.0f, 360.0f), 0.9f, 1.0f);
        DrawTextPro(font, c, (Vector2){ x, y }, (Vector2){ 0, 0 }, rot, fontSize, spacing, col);
        x += MeasureTextEx(font, c, fontSize, spacing).x;
    }
}

//render do menu principal

MainMenuResult UIDrawMainMenu(Texture2D imgMenu, float t) {
    MainMenuResult result = { 0 };

    ClearBackground(BLACK);
    DrawTexturePro(imgMenu,
        (Rectangle){0,0,(float)imgMenu.width,(float)imgMenu.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0}, 0, WHITE);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade((Color){20, 20, 26, 255}, 0.35f));

    DrawGoofyTitle("Le bombo plane experience", t);
    float bw = 220.0f;
    float bh = 84.0f;
    float margin = 26.0f;
    Rectangle leaveBtn = {
        margin,
        GetScreenHeight() - bh - margin,
        bw,
        bh
    };
    Rectangle playBtn = {
        GetScreenWidth() - bw - margin,
        GetScreenHeight() - bh - margin,
        bw,
        bh
    };

    result.leave = DrawGoofyButton(leaveBtn, "LEAVE", (Color){150, 50, 60, 255}, t);
    result.play  = DrawGoofyButton(playBtn,  "PLAY",  (Color){45, 140, 85, 255}, t);
    return result;
}

//render da camara no preview menu

VehicleMenuResult UIDrawVehicleMenuUI(float t, float uiOffsetY,
                                      VehicleType previewVehicle,
                                      bool menuSliding) {
    VehicleMenuResult result = { 0 };

    DrawGoofyTitle("Le bombo plane experience", t);

    Rectangle leftArrow = { 52.0f, GetScreenHeight() * 0.5f - 58.0f + uiOffsetY, 92.0f, 116.0f };
    Rectangle rightArrow = { GetScreenWidth() - 144.0f, GetScreenHeight() * 0.5f - 58.0f + uiOffsetY, 92.0f, 116.0f };

    result.requestPrev = (!menuSliding && DrawArrowButton(leftArrow, true, t)) ||
                         (!menuSliding && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)));

    result.requestNext = (!menuSliding && DrawArrowButton(rightArrow, false, t)) ||
                         (!menuSliding && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)));

    DrawText("Choose a vehicle", GetScreenWidth()/2 - 145, 128 + (int)uiOffsetY, 34, WHITE);
    DrawText(TextFormat("VEHICLE: %s", GetVehicleName(previewVehicle)),
             GetScreenWidth()/2 - 145, 510 + (int)uiOffsetY, 34, YELLOW);
    DrawText("Use arrows or A/D to switch", GetScreenWidth()/2 - 188, 548 + (int)uiOffsetY, 24, RAYWHITE);

    Rectangle playVehicleBtn = {
        GetScreenWidth() * 0.5f - 150.0f,
        GetScreenHeight() - 94.0f,
        300.0f,
        72.0f
    };
    Rectangle backVehicleBtn = {
        40.0f,
        GetScreenHeight() - 94.0f,
        220.0f,
        72.0f
    };

    result.back = DrawGoofyButton(backVehicleBtn, "BACK", (Color){70, 90, 150, 255}, t);
    result.play = (!menuSliding && DrawGoofyButton(playVehicleBtn, "PLAY", (Color){45, 140, 85, 255}, t));

    return result;
}

//lose screen

void UIDrawLoseScreen(Texture2D imgLose, bool blinkOn) {
    ClearBackground(BLACK);
    DrawTexturePro(imgLose,
        (Rectangle){0,0,(float)imgLose.width,(float)imgLose.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0}, 0, WHITE);
    if (blinkOn) {
        const char* msg = "Press X to exit, R to restart, M for menu";
        int tw = MeasureText(msg, 28);
        DrawText(msg, GetScreenWidth()/2 - tw/2,
                 GetScreenHeight()/2 + 80, 28, YELLOW);
    }
}

//overlays imagem gameplay

void UIDrawGameplayOverlays(Texture2D imgBomb, Texture2D imgGoldenHit,
                            Texture2D imgScared, Texture2D imgKirk,
                            Texture2D imgDima, Texture2D img3p, Texture2D imgChicken,
                            Texture2D imgNk, Texture2D imgMachine,
                            Texture2D imgSp,
                            float laserAlpha, float goldenHitAlpha,
                            float scaredAlpha, float kirkAlpha,
                            float dimaAlpha, float epAlpha, float chickenAlpha,
                            float nkAlpha, float machineAlpha,
                            float spFadeAlpha, float nukeCoverAlpha) {
    if (laserAlpha   > 0) DrawTexturePro(imgBomb,
        (Rectangle){0,0,(float)imgBomb.width,(float)imgBomb.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,laserAlpha));
    if (goldenHitAlpha > 0) DrawTexturePro(imgGoldenHit,
        (Rectangle){0,0,(float)imgGoldenHit.width,(float)imgGoldenHit.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,goldenHitAlpha));
    if (scaredAlpha  > 0) DrawTexturePro(imgScared,
        (Rectangle){0,0,(float)imgScared.width,(float)imgScared.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,scaredAlpha));
    if (kirkAlpha    > 0) DrawTexturePro(imgKirk,
        (Rectangle){0,0,(float)imgKirk.width,(float)imgKirk.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,kirkAlpha));
    if (dimaAlpha    > 0) DrawTexturePro(imgDima,
        (Rectangle){0,0,(float)imgDima.width,(float)imgDima.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,dimaAlpha));
    if (epAlpha      > 0) DrawTexturePro(img3p,
        (Rectangle){0,0,(float)img3p.width,(float)img3p.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,epAlpha));
    if (chickenAlpha > 0) DrawTexturePro(imgChicken,
        (Rectangle){0,0,(float)imgChicken.width,(float)imgChicken.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,chickenAlpha));
    if (nkAlpha      > 0) DrawTexturePro(imgNk,
        (Rectangle){0,0,(float)imgNk.width,(float)imgNk.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,nkAlpha));
    if (machineAlpha > 0) DrawTexturePro(imgMachine,
        (Rectangle){0,0,(float)imgMachine.width,(float)imgMachine.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,machineAlpha));
    if (spFadeAlpha > 0) DrawTexturePro(imgSp,
        (Rectangle){0,0,(float)imgSp.width,(float)imgSp.height},
        (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
        (Vector2){0,0},0,Fade(WHITE,spFadeAlpha));
    if (nukeCoverAlpha > 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade((Color){255, 120, 0, 255}, nukeCoverAlpha * 0.75f));
    }
}

//HUD gameplay

void UIDrawGameplayHUD(int score, bool blinkOn,
                       int bombCount, float bombRegenTimer,
                       bool nukeUsed, float machineGunCooldown,
                       bool machineGunActive, float spaceHeldTime,
                       bool nukeActive, bool nukeWaitingImpact,
                       float nukeAlertTimer, bool nukeRainActive,
                       int comboLevel, int comboHitStreak) {
    if (nukeActive || nukeWaitingImpact) {
        bool alertOn = sinf(nukeAlertTimer) > 0.0f;
        int fs = alertOn ? 56 : 48;
        const char* nukeMsg = nukeWaitingImpact ?
                              "NUKE IMPACT - STAND BY" :
                              "NUCLEAR STRIKE INCOMING";
        int tw = MeasureText(nukeMsg, fs);
        DrawText(nukeMsg, GetScreenWidth()/2 - tw/2, 26, fs,
                 alertOn ? RED : ORANGE);
    }

    if (nukeRainActive) {
        int tw = MeasureText("DEBRIS RAIN IN PROGRESS", 42);
        DrawText("DEBRIS RAIN IN PROGRESS",
                 GetScreenWidth()/2 - tw/2, 92, 42,
                 Fade(ORANGE, 0.92f));
    }

    //bloco de combo (topo direito)
    int comboW = 250;
    int comboH = 110;
    int comboX = GetScreenWidth() - comboW - 12;
    int comboY = 10;
    DrawRectangle(comboX, comboY, comboW, comboH, Fade(BLACK, 0.58f));
    DrawRectangleLinesEx((Rectangle){ (float)comboX, (float)comboY, (float)comboW, (float)comboH }, 2.0f, Fade(RAYWHITE, 0.75f));
    DrawText("COMBO", comboX + 14, comboY + 10, 24, RAYWHITE);

    int comboMultiplier = comboLevel + 1;
    Color comboColor = LIGHTGRAY;
    if (comboLevel == 1) comboColor = YELLOW;
    else if (comboLevel == 2) comboColor = ORANGE;
    else if (comboLevel == 3) comboColor = RED;

    if (comboLevel == 3) {
        float t = (float)GetTime();
        float pulse = 1.0f + sinf(t * 9.0f) * 0.16f;
        comboColor = ColorFromHSV(fmodf(t * 250.0f, 360.0f), 0.95f, 1.0f);
        int fs = (int)(48.0f * pulse);
        DrawText(TextFormat("x%d", comboLevel), comboX + 16, comboY + 38, fs, comboColor);
    } else {
        DrawText(TextFormat("x%d", comboLevel), comboX + 16, comboY + 42, 44, comboColor);
    }
    DrawText(TextFormat("HITS: %d", comboHitStreak), comboX + 118, comboY + 46, 24, comboColor);
    DrawText(TextFormat("SCORE x%d", comboMultiplier), comboX + 118, comboY + 76, 20, Fade(comboColor, 0.95f));

    DrawRectangle(10,10,220,40,Fade(BLACK,0.5f));
    DrawText(TextFormat("SCORE: %05i",score),20,20,20,blinkOn?YELLOW:RED);

    DrawRectangle(10,58,220,52,Fade(BLACK,0.5f));
    Color bClr = blinkOn ? YELLOW : RED;
    DrawText("BOMBS:",20,68,18,bClr);
    for (int i = 0; i < MAX_BOMBS; i++) {
        int bx = 120+i*30;
        if (i < bombCount) DrawCircle(bx,79,9,bClr);
        else DrawCircleLines(bx,79,9,Fade(bClr,0.35f));
    }

    if (bombCount < MAX_BOMBS) {
        float p = bombRegenTimer / BOMB_REGEN_TIME;
        DrawRectangle(20,98,190,5,Fade(BLACK,0.6f));
        DrawRectangle(20,98,(int)(190.0f*p),5,bClr);
    }

    DrawText(TextFormat("NUKE[F]: %s", nukeUsed ? "USED" : "READY"),
             20, 108, 16, nukeUsed ? LIGHTGRAY : ORANGE);

    if (machineGunCooldown > 0.0f) {
        float p = machineGunCooldown / MACHINE_GUN_COOLDOWN;
        DrawRectangle(10,118,220,18,Fade(BLACK,0.5f));
        DrawRectangle(10,118,(int)(220.0f*(1.0f-p)),18,Fade(RED,0.8f));
        DrawText("COOLDOWN",70,119,14,WHITE);
    }

    if (machineGunActive) {
        DrawRectangle(10,140,220,22,Fade(RED,0.8f));
        DrawText("MACHINE GUN ACTIVE",15,143,16,WHITE);
    } else if (spaceHeldTime > 0.0f && spaceHeldTime < MACHINE_GUN_ACTIVATE_TIME) {
        float p = spaceHeldTime / MACHINE_GUN_ACTIVATE_TIME;
        DrawRectangle(10,140,220,22,Fade(BLACK,0.5f));
        DrawRectangle(10,140,(int)(220.0f*p),22,Fade(ORANGE,0.8f));
        DrawText("HOLD... MG",70,143,16,WHITE);
    }

    DrawRectangle(0,GetScreenHeight()-40,GetScreenWidth(),40,Fade(BLACK,0.7f));
    DrawText(TextFormat("O:CAM | SPACE:LASER/MG(%.1fs) | B:BOMB | F:NUKE(1x) | SHIFT:SLOW | CTRL:BOOST | X:EXIT | M:MENU",
                        MACHINE_GUN_ACTIVATE_TIME),
             6,GetScreenHeight()-28,15,YELLOW);
}

//first person crosshair

void UIDrawCrosshair(void) {
    DrawCircleLines(GetScreenWidth()/2,GetScreenHeight()/2,10,LIME);
    DrawLine(GetScreenWidth()/2-20,GetScreenHeight()/2,
             GetScreenWidth()/2+20,GetScreenHeight()/2,LIME);
    DrawLine(GetScreenWidth()/2,GetScreenHeight()/2-20,
             GetScreenWidth()/2,GetScreenHeight()/2+20,LIME);
}
