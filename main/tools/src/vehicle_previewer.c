#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "obj.h"

static bool DrawArrowSide(Rectangle rect, bool left) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRec(rect, hover ? Fade((Color){30, 30, 40, 255}, 0.95f)
                                 : Fade((Color){16, 16, 22, 255}, 0.88f));
    DrawRectangleLinesEx(rect, 3.0f, hover ? YELLOW : RAYWHITE);

    float dir = left ? -1.0f : 1.0f;
    Vector2 c = { rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f };
    Vector2 tip = { c.x + dir * 16.0f, c.y };
    Vector2 up = { c.x - dir * 12.0f, c.y - 22.0f };
    Vector2 down = { c.x - dir * 12.0f, c.y + 22.0f };
    DrawTriangle(up, tip, down, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static float GetSpinnerSpeed(VehicleType vehicle) {
    if (vehicle == VEHICLE_HELICOPTER) return HELICOPTER_ROTOR_SPEED;
    if (vehicle == VEHICLE_JET) return PROPELLER_SPEED * 1.6f;
    return PROPELLER_SPEED;
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1100, 700, "Vehicle Previewer");
    SetTargetFPS(60);

    VehicleType current = VEHICLE_AIRPLANE;
    float spinner = 0.0f;

    Camera3D cam = { 0 };
    cam.position = (Vector3){ 0.0f, 7.0f, 28.0f };
    cam.target = (Vector3){ 0.0f, 1.8f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 36.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        float t = (float)GetTime();
        spinner += GetSpinnerSpeed(current) * GetFrameTime() * 60.0f;

        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            current = (VehicleType)((current + VEHICLE_COUNT - 1) % VEHICLE_COUNT);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            current = (VehicleType)((current + 1) % VEHICLE_COUNT);
        }

        Rectangle leftArrow = { 44.0f, GetScreenHeight() * 0.5f - 56.0f, 88.0f, 112.0f };
        Rectangle rightArrow = { GetScreenWidth() - 132.0f, GetScreenHeight() * 0.5f - 56.0f, 88.0f, 112.0f };

        if (DrawArrowSide(leftArrow, true)) {
            current = (VehicleType)((current + VEHICLE_COUNT - 1) % VEHICLE_COUNT);
        }
        if (DrawArrowSide(rightArrow, false)) {
            current = (VehicleType)((current + 1) % VEHICLE_COUNT);
        }

        BeginDrawing();
            ClearBackground((Color){ 9, 12, 20, 255 });

            BeginMode3D(cam);
                DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 220, 220 }, (Color){ 22, 36, 35, 255 });
                DrawGrid(40, 3.2f);

                rlPushMatrix();
                    float previewYOffset = 2.6f;
                    if (current == VEHICLE_UFO) previewYOffset -= 0.6f;
                    rlTranslatef(0, previewYOffset, 0);
                    rlRotatef(165.0f + sinf(t * 0.7f) * 8.0f, 0, 1, 0);
                    DrawVehicleModel(current, spinner);
                rlPopMatrix();
            EndMode3D();

            DrawRectangle(0, 0, GetScreenWidth(), 64, Fade(BLACK, 0.35f));
            DrawRectangle(0, GetScreenHeight() - 84, GetScreenWidth(), 84, Fade(BLACK, 0.45f));

            int fs = 34;
            const char *name = GetVehicleName(current);
            int tw = MeasureText(name, fs);
            DrawText(name, GetScreenWidth() / 2 - tw / 2, GetScreenHeight() - 56, fs, YELLOW);
            DrawText("LEFT/RIGHT or A/D", GetScreenWidth() / 2 - 128, 22, 24, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
