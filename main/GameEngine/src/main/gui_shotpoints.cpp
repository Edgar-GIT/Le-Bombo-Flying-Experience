#include "../include/gui_shotpoints.hpp"
#include "../include/gui_manager.hpp"
#include "../include/gui_workflow.hpp"
#include "../include/persistence.hpp"

#include "raymath.h"

#include <cmath>
#include <cstdio>

// build object matrix in translate rotate scale order
static Matrix BuildObjectMatrix(const EditorObject &obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixTranslate(obj.position.x, obj.position.y, obj.position.z));
    mat = MatrixMultiply(mat, MatrixRotateX(obj.rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj.rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj.rotation.z * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixScale(obj.scale.x, obj.scale.y, obj.scale.z));
    return mat;
}

// build rotation only matrix for one object local axes
static Matrix BuildObjectRotationMatrix(const EditorObject &obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixRotateX(obj.rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj.rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj.rotation.z * DEG2RAD));
    return mat;
}

// returns world axis direction from selected gizmo axis
static Vector3 AxisVectorFromGizmoAxis(GizmoAxis axis) {
    if (axis == GizmoAxis::X) return (Vector3){ 1.0f, 0.0f, 0.0f };
    if (axis == GizmoAxis::Y) return (Vector3){ 0.0f, 1.0f, 0.0f };
    if (axis == GizmoAxis::Z) return (Vector3){ 0.0f, 0.0f, 1.0f };
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

// detects hovered gizmo axis around marker in placement window
static GizmoAxis DetectPlacementAxis(const EditorGuiState &st, Vector3 markerPos, Vector3 axisX, Vector3 axisY, Vector3 axisZ) {
    Vector2 mouse = GetMousePosition();
    Vector2 s = GetWorldToScreen(markerPos, st.shotpointPlacementCamera);
    Vector2 x = GetWorldToScreen(Vector3Add(markerPos, axisX), st.shotpointPlacementCamera);
    Vector2 y = GetWorldToScreen(Vector3Add(markerPos, axisY), st.shotpointPlacementCamera);
    Vector2 z = GetWorldToScreen(Vector3Add(markerPos, axisZ), st.shotpointPlacementCamera);

    float dx = DistancePointToSegment(mouse, s, x);
    float dy = DistancePointToSegment(mouse, s, y);
    float dz = DistancePointToSegment(mouse, s, z);

    float best = 16.0f;
    GizmoAxis axis = GizmoAxis::None;
    if (dx < best) {
        best = dx;
        axis = GizmoAxis::X;
    }
    if (dy < best) {
        best = dy;
        axis = GizmoAxis::Y;
    }
    if (dz < best) axis = GizmoAxis::Z;
    return axis;
}

// clamp selected shotpoint index after list operations
static void ClampSelectedShotpoint(EditorGuiState &st) {
    if (st.shotpoints.empty()) {
        st.selectedShotpoint = -1;
        return;
    }
    if (st.selectedShotpoint < 0) st.selectedShotpoint = 0;
    if (st.selectedShotpoint >= (int)st.shotpoints.size()) st.selectedShotpoint = (int)st.shotpoints.size() - 1;
}

// create default shotpoint and bind to selected block when possible
static void AddShotpoint(EditorGuiState &st) {
    Shotpoint sp = {};
    char name[64] = { 0 };
    std::snprintf(name, sizeof(name), "SP_%02d", (int)st.shotpoints.size() + 1);
    sp.name = name;
    sp.objectIndex = (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) ? st.selectedIndex : -1;
    sp.enabled = true;
    sp.localPos = (Vector3){ 0.0f, 0.0f, 0.0f };
    sp.blastColor = (Color){ 240, 90, 90, 255 };
    sp.blastSize = 0.16f;
    st.shotpoints.push_back(sp);
    st.selectedShotpoint = (int)st.shotpoints.size() - 1;
    AddLog(st, "shotpoint created");
    RecordCacheAction(st, "shotpoint_add", "created shotpoint");
    MarkProjectDirty(st);
}

// delete selected shotpoint
static void DeleteSelectedShotpoint(EditorGuiState &st) {
    if (st.selectedShotpoint < 0 || st.selectedShotpoint >= (int)st.shotpoints.size()) return;
    st.shotpoints.erase(st.shotpoints.begin() + st.selectedShotpoint);
    ClampSelectedShotpoint(st);
    AddLog(st, "shotpoint deleted");
    RecordCacheAction(st, "shotpoint_delete", "deleted shotpoint");
    MarkProjectDirty(st);
}

// draw integer color control with plus minus buttons
static bool DrawColorStepper(const char *label, int *value, int x, int y) {
    DrawText(label, x, y + 4, 16, (Color){ 198, 206, 219, 255 });
    Rectangle minusRect = { (float)x + 34.0f, (float)y, 22.0f, 22.0f };
    Rectangle plusRect = { (float)x + 120.0f, (float)y, 22.0f, 22.0f };
    Rectangle valueRect = { (float)x + 60.0f, (float)y, 56.0f, 22.0f };
    bool changed = false;

    if (DrawButton(minusRect, "-", "decrease value", false, 14)) {
        *value -= 5;
        changed = true;
    }
    if (DrawButton(plusRect, "+", "increase value", false, 14)) {
        *value += 5;
        changed = true;
    }
    if (*value < 0) *value = 0;
    if (*value > 255) *value = 255;

    DrawRectangleRounded(valueRect, 0.14f, 6, (Color){ 35, 40, 49, 255 });
    DrawRectangleRoundedLinesEx(valueRect, 0.14f, 6, 1.0f, (Color){ 81, 89, 104, 255 });
    DrawText(TextFormat("%d", *value), x + 74, y + 4, 16, RAYWHITE);
    return changed;
}

// draw float control with plus minus buttons
static bool DrawFloatStepper(const char *label, float *value, float step, float minV, float maxV, int x, int y, int width) {
    DrawText(label, x, y + 4, 16, (Color){ 198, 206, 219, 255 });
    Rectangle minusRect = { (float)x + 66.0f, (float)y, 24.0f, 22.0f };
    Rectangle plusRect = { (float)x + (float)width - 24.0f, (float)y, 24.0f, 22.0f };
    Rectangle valueRect = { (float)x + 94.0f, (float)y, (float)width - 122.0f, 22.0f };
    bool changed = false;

    if (DrawButton(minusRect, "-", "decrease value", false, 14)) {
        *value -= step;
        changed = true;
    }
    if (DrawButton(plusRect, "+", "increase value", false, 14)) {
        *value += step;
        changed = true;
    }
    *value = Clampf(*value, minV, maxV);

    DrawRectangleRounded(valueRect, 0.14f, 6, (Color){ 35, 40, 49, 255 });
    DrawRectangleRoundedLinesEx(valueRect, 0.14f, 6, 1.0f, (Color){ 81, 89, 104, 255 });
    DrawText(TextFormat("%.2f", *value), x + 104, y + 4, 16, RAYWHITE);
    return changed;
}

// open dedicated placement view for selected shotpoint
static void OpenShotpointPlacement(EditorGuiState &st) {
    if (st.selectedShotpoint < 0 || st.selectedShotpoint >= (int)st.shotpoints.size()) return;
    Shotpoint &sp = st.shotpoints[st.selectedShotpoint];
    if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) {
        AddLog(st, "bind a block before placement");
        return;
    }

    st.shotpointPlacementOpen = true;
    st.shotpointPlacementIndex = st.selectedShotpoint;

    const EditorObject &obj = st.objects[sp.objectIndex];
    st.shotpointPlacementCamera = {};
    st.shotpointPlacementCamera.position = Vector3Add(obj.position, (Vector3){ 0.0f, 2.0f, 8.0f });
    st.shotpointPlacementCamera.target = obj.position;
    st.shotpointPlacementCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    st.shotpointPlacementCamera.fovy = 60.0f;
    st.shotpointPlacementCamera.projection = CAMERA_PERSPECTIVE;

    Vector3 look = Vector3Normalize(Vector3Subtract(st.shotpointPlacementCamera.target, st.shotpointPlacementCamera.position));
    st.shotpointPlacementYaw = atan2f(look.x, look.z);
    st.shotpointPlacementPitch = asinf(Clampf(look.y, -1.0f, 1.0f));
    st.shotpointPlacementAxis = GizmoAxis::None;
    st.shotpointPlacementDrag = false;
}

void DrawShotpointsTab(Rectangle panelRect, EditorGuiState &st) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Lasers / Shotpoints", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    Rectangle addBtn = { panelRect.x + 10.0f, panelRect.y + 40.0f, 68.0f, 24.0f };
    Rectangle delBtn = { panelRect.x + 84.0f, panelRect.y + 40.0f, 68.0f, 24.0f };
    Rectangle previewAllBtn = { panelRect.x + 158.0f, panelRect.y + 40.0f, 132.0f, 24.0f };
    if (DrawButton(addBtn, "Add", "add new shotpoint", false, 15)) AddShotpoint(st);
    if (DrawButton(delBtn, "Delete", "delete selected shotpoint", false, 15)) DeleteSelectedShotpoint(st);
    if (DrawButton(previewAllBtn, "Preview Blasts", "open preview with all shotpoints firing", false, 15)) {
        OpenBlastPreview(st, -1);
    }

    ClampSelectedShotpoint(st);

    Rectangle listArea = { panelRect.x + 8.0f, panelRect.y + 72.0f, panelRect.width - 16.0f, 112.0f };
    DrawRectangleRounded(listArea, 0.06f, 6, (Color){ 29, 33, 40, 255 });
    DrawRectangleRoundedLinesEx(listArea, 0.06f, 6, 1.0f, (Color){ 78, 86, 101, 255 });

    float rowY = listArea.y + 6.0f;
    for (int i = 0; i < (int)st.shotpoints.size(); i++) {
        Shotpoint &sp = st.shotpoints[i];
        Rectangle row = { listArea.x + 6.0f, rowY, listArea.width - 12.0f, 24.0f };
        bool selected = (i == st.selectedShotpoint);
        DrawRectangleRounded(row, 0.10f, 4, selected ? (Color){ 76, 94, 126, 255 } : (Color){ 45, 50, 60, 255 });
        DrawRectangleRoundedLinesEx(row, 0.10f, 4, 1.0f, (Color){ 83, 92, 108, 255 });
        DrawText(sp.name.c_str(), (int)row.x + 8, (int)row.y + 4, 16, RAYWHITE);
        DrawText(sp.enabled ? "on" : "off", (int)(row.x + row.width - 34.0f), (int)row.y + 4, 15, sp.enabled ? GREEN : RED);
        if (CheckCollisionPointRec(GetMousePosition(), row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            st.selectedShotpoint = i;
        }
        rowY += 28.0f;
        if (rowY > listArea.y + listArea.height - 24.0f) break;
    }

    if (st.shotpoints.empty()) {
        DrawText("No shotpoints yet", (int)listArea.x + 10, (int)listArea.y + 10, 16, (Color){ 184, 191, 204, 255 });
        return;
    }

    if (st.selectedShotpoint < 0 || st.selectedShotpoint >= (int)st.shotpoints.size()) return;
    Shotpoint &sp = st.shotpoints[st.selectedShotpoint];

    float x = panelRect.x + 10.0f;
    float y = panelRect.y + 192.0f;
    int width = (int)panelRect.width - 20;

    DrawText("Target block", (int)x, (int)y, 18, (Color){ 220, 227, 238, 255 });
    y += 24.0f;
    const char *boundName = (sp.objectIndex >= 0 && sp.objectIndex < (int)st.objects.size()) ? st.objects[sp.objectIndex].name.c_str() : "none";
    DrawRectangleRounded((Rectangle){ x, y, (float)width, 24.0f }, 0.12f, 6, (Color){ 33, 39, 47, 255 });
    DrawRectangleRoundedLinesEx((Rectangle){ x, y, (float)width, 24.0f }, 0.12f, 6, 1.0f, (Color){ 84, 92, 108, 255 });
    DrawText(boundName, (int)x + 8, (int)y + 4, 16, RAYWHITE);
    y += 30.0f;

    Rectangle bindBtn = { x, y, 138.0f, 24.0f };
    Rectangle placeBtn = { x + 146.0f, y, 144.0f, 24.0f };
    if (DrawButton(bindBtn, "Bind Selected", "bind shotpoint to selected block", false, 15)) {
        if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
            sp.objectIndex = st.selectedIndex;
            sp.localPos = (Vector3){ 0.0f, 0.0f, 0.0f };
            AddLog(st, "shotpoint bound to selected block");
            MarkProjectDirty(st);
        } else {
            AddLog(st, "select one block to bind");
        }
    }
    if (DrawButton(placeBtn, "Define Placement", "open isolated placement view", false, 15)) {
        OpenShotpointPlacement(st);
    }
    y += 30.0f;

    Rectangle prevBtn = { x, y, 160.0f, 24.0f };
    Rectangle enableBtn = { x + 168.0f, y, 122.0f, 24.0f };
    if (DrawButton(prevBtn, "Preview Shot", "preview selected shotpoint firing", false, 15)) {
        OpenBlastPreview(st, st.selectedShotpoint);
    }
    if (DrawButton(enableBtn, sp.enabled ? "Enabled" : "Disabled", "toggle shotpoint output", sp.enabled, 15)) {
        sp.enabled = !sp.enabled;
        MarkProjectDirty(st);
    }
    y += 34.0f;

    int cr = sp.blastColor.r;
    int cg = sp.blastColor.g;
    int cb = sp.blastColor.b;
    bool colorChanged = false;
    colorChanged |= DrawColorStepper("R", &cr, (int)x, (int)y);
    colorChanged |= DrawColorStepper("G", &cg, (int)x + 98, (int)y);
    colorChanged |= DrawColorStepper("B", &cb, (int)x + 196, (int)y);
    if (colorChanged) {
        sp.blastColor.r = (unsigned char)cr;
        sp.blastColor.g = (unsigned char)cg;
        sp.blastColor.b = (unsigned char)cb;
        MarkProjectDirty(st);
    }
    y += 28.0f;

    if (DrawFloatStepper("Size", &sp.blastSize, 0.02f, 0.05f, 1.8f, (int)x, (int)y, width)) {
        MarkProjectDirty(st);
    }
    y += 30.0f;

    DrawRectangle((int)x, (int)y, width, 28, sp.blastColor);
    DrawRectangleLines((int)x, (int)y, width, 28, BLACK);
    DrawText("blast color", (int)x + 8, (int)y + 6, 15, BLACK);
}

void DrawShotpointPlacementWindow(EditorGuiState &st, float dt) {
    if (!st.shotpointPlacementOpen) return;
    if (st.shotpointPlacementIndex < 0 || st.shotpointPlacementIndex >= (int)st.shotpoints.size()) {
        st.shotpointPlacementOpen = false;
        return;
    }

    Shotpoint &sp = st.shotpoints[st.shotpointPlacementIndex];
    if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) {
        st.shotpointPlacementOpen = false;
        return;
    }

    EditorObject &obj = st.objects[sp.objectIndex];

    if (IsKeyPressed(KEY_ESCAPE)) {
        st.shotpointPlacementOpen = false;
        st.shotpointPlacementDrag = false;
        st.shotpointPlacementAxis = GizmoAxis::None;
        return;
    }

    Vector3 lookDir = Vector3Normalize(Vector3Subtract(st.shotpointPlacementCamera.target, st.shotpointPlacementCamera.position));
    if (Vector3LengthSqr(lookDir) < 0.0001f) lookDir = (Vector3){ 0.0f, 0.0f, -1.0f };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(lookDir, (Vector3){ 0.0f, 1.0f, 0.0f }));
    if (Vector3LengthSqr(right) < 0.0001f) right = (Vector3){ 1.0f, 0.0f, 0.0f };
    Vector3 up = (Vector3){ 0.0f, 1.0f, 0.0f };

    // update camera look controls
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        Vector2 d = GetMouseDelta();
        st.shotpointPlacementYaw += d.x * st.previewLookSensitivity;
        st.shotpointPlacementPitch -= d.y * st.previewLookSensitivity;
    }
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        float pan = st.previewMoveSpeed * st.previewPanSensitivity * dt;
        st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(right, -d.x * pan));
        st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(up, d.y * pan));
    }

    float wheel = GetMouseWheelMove();
    if (fabsf(wheel) > 0.0001f) {
        st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(lookDir, wheel * st.previewZoomSpeed));
    }

    float keyLook = st.previewLookSensitivity * 200.0f * dt;
    if (IsKeyDown(KEY_LEFT)) st.shotpointPlacementYaw -= keyLook;
    if (IsKeyDown(KEY_RIGHT)) st.shotpointPlacementYaw += keyLook;
    if (IsKeyDown(KEY_UP)) st.shotpointPlacementPitch += keyLook;
    if (IsKeyDown(KEY_DOWN)) st.shotpointPlacementPitch -= keyLook;

    float speedMul = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 2.5f : 1.0f;
    float move = st.previewMoveSpeed * speedMul * dt;
    Vector3 flatForward = Vector3Normalize((Vector3){ lookDir.x, 0.0f, lookDir.z });
    if (Vector3LengthSqr(flatForward) < 0.0001f) flatForward = (Vector3){ 0.0f, 0.0f, -1.0f };
    if (IsKeyDown(KEY_W)) st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(flatForward, move));
    if (IsKeyDown(KEY_S)) st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(flatForward, -move));
    if (IsKeyDown(KEY_A)) st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(right, -move));
    if (IsKeyDown(KEY_D)) st.shotpointPlacementCamera.position = Vector3Add(st.shotpointPlacementCamera.position, Vector3Scale(right, move));

    st.shotpointPlacementPitch = Clampf(st.shotpointPlacementPitch, -1.53f, 1.53f);
    Vector3 forward = {
        cosf(st.shotpointPlacementPitch) * sinf(st.shotpointPlacementYaw),
        sinf(st.shotpointPlacementPitch),
        cosf(st.shotpointPlacementPitch) * cosf(st.shotpointPlacementYaw)
    };
    st.shotpointPlacementCamera.target = Vector3Add(st.shotpointPlacementCamera.position, forward);
    st.shotpointPlacementCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };

    Matrix objMat = BuildObjectMatrix(obj);
    Matrix objInv = MatrixInvert(objMat);
    Matrix objRot = BuildObjectRotationMatrix(obj);
    Vector3 markerPos = Vector3Transform(sp.localPos, objMat);
    Vector3 markerDir = Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, objRot);
    if (Vector3LengthSqr(markerDir) < 0.0001f) markerDir = (Vector3){ 0.0f, 0.0f, -1.0f };
    markerDir = Vector3Normalize(markerDir);

    Vector3 axisX = Vector3Normalize(Vector3Transform((Vector3){ 1.4f, 0.0f, 0.0f }, objRot));
    Vector3 axisY = Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 1.4f, 0.0f }, objRot));
    Vector3 axisZ = Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 0.0f, 1.4f }, objRot));

    GizmoAxis hoverAxis = DetectPlacementAxis(st, markerPos, axisX, axisY, axisZ);

    // starts axis dragging or object click placement
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && GetMousePosition().y > 46.0f) {
        if (hoverAxis != GizmoAxis::None) {
            st.shotpointPlacementAxis = hoverAxis;
            st.shotpointPlacementDrag = true;
        } else {
            Ray ray = GetMouseRay(GetMousePosition(), st.shotpointPlacementCamera);
            float radius = fmaxf(fmaxf(obj.scale.x, obj.scale.y), obj.scale.z) * 0.70f;
            if (radius < 0.25f) radius = 0.25f;
            RayCollision hit = GetRayCollisionSphere(ray, obj.position, radius);
            if (hit.hit) {
                sp.localPos = Vector3Transform(hit.point, objInv);
                AddLog(st, "shotpoint placement updated");
                RecordCacheAction(st, "shotpoint_place", "updated shotpoint placement");
                MarkProjectDirty(st);
            }
        }
    }

    // applies gizmo drag movement to shotpoint local placement
    if (st.shotpointPlacementDrag && st.shotpointPlacementAxis != GizmoAxis::None && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector3 axisWorld = axisX;
        if (st.shotpointPlacementAxis == GizmoAxis::Y) axisWorld = axisY;
        else if (st.shotpointPlacementAxis == GizmoAxis::Z) axisWorld = axisZ;

        Vector2 p0 = GetWorldToScreen(markerPos, st.shotpointPlacementCamera);
        Vector2 p1 = GetWorldToScreen(Vector3Add(markerPos, axisWorld), st.shotpointPlacementCamera);
        Vector2 screenAxis = Vector2Subtract(p1, p0);
        float screenLen = Vector2Length(screenAxis);
        if (screenLen > 0.0001f) {
            Vector2 dir = Vector2Scale(screenAxis, 1.0f / screenLen);
            float pixels = Vector2DotProduct(GetMouseDelta(), dir);
            float dist = Vector3Distance(st.shotpointPlacementCamera.position, markerPos);
            float worldDelta = pixels * (0.0022f * dist) * st.gizmoMoveSensitivity;
            Vector3 movedWorld = Vector3Add(markerPos, Vector3Scale(axisWorld, worldDelta));
            sp.localPos = Vector3Transform(movedWorld, objInv);
            MarkProjectDirty(st);
        }
    }

    if (st.shotpointPlacementDrag && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        st.shotpointPlacementDrag = false;
        st.shotpointPlacementAxis = GizmoAxis::None;
    }

    markerPos = Vector3Transform(sp.localPos, objMat);
    markerDir = Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, objRot);
    if (Vector3LengthSqr(markerDir) < 0.0001f) markerDir = (Vector3){ 0.0f, 0.0f, -1.0f };
    markerDir = Vector3Normalize(markerDir);

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, BLACK);
    BeginMode3D(st.shotpointPlacementCamera);
        DrawPlane((Vector3){ 0, -0.01f, 0 }, (Vector2){ 180.0f, 180.0f }, (Color){ 8, 8, 8, 255 });
        DrawGrid(120, 1.0f);
        DrawEditorObject3D(obj);
        float blink = 0.4f + 0.6f * fabsf(sinf((float)GetTime() * 5.0f));
        DrawSphere(markerPos, 0.11f, Fade(sp.blastColor, blink));
        DrawCylinderEx(markerPos, Vector3Add(markerPos, Vector3Scale(markerDir, 3.5f)), sp.blastSize, sp.blastSize * 0.8f, 8, Fade(sp.blastColor, 0.80f));

        Color xC = st.shotpointPlacementAxis == GizmoAxis::X ? (Color){ 255, 150, 150, 255 } : (Color){ 240, 82, 82, 255 };
        Color yC = st.shotpointPlacementAxis == GizmoAxis::Y ? (Color){ 160, 255, 160, 255 } : (Color){ 92, 217, 108, 255 };
        Color zC = st.shotpointPlacementAxis == GizmoAxis::Z ? (Color){ 156, 200, 255, 255 } : (Color){ 88, 146, 245, 255 };
        DrawLine3D(markerPos, Vector3Add(markerPos, axisX), xC);
        DrawLine3D(markerPos, Vector3Add(markerPos, axisY), yC);
        DrawLine3D(markerPos, Vector3Add(markerPos, axisZ), zC);
        DrawSphere(Vector3Add(markerPos, axisX), 0.06f, xC);
        DrawSphere(Vector3Add(markerPos, axisY), 0.06f, yC);
        DrawSphere(Vector3Add(markerPos, axisZ), 0.06f, zC);
    EndMode3D();

    DrawRectangle(0, 0, sw, 44, (Color){ 15, 18, 24, 230 });
    DrawLine(0, 44, sw, 44, (Color){ 80, 88, 103, 255 });
    DrawText("Shotpoint Placement", 12, 12, 21, RAYWHITE);
    DrawText("RMB look | MMB pan | wheel zoom | click object to place | drag XYZ gizmo", 240, 14, 16, (Color){ 190, 199, 214, 255 });
    if (DrawButton((Rectangle){ (float)sw - 112.0f, 8.0f, 98.0f, 28.0f }, "Done", "close placement view")) {
        st.shotpointPlacementOpen = false;
        st.shotpointPlacementDrag = false;
        st.shotpointPlacementAxis = GizmoAxis::None;
    }
}
