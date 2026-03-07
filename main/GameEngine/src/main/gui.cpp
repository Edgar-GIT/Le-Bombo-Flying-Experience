#include "../include/gui.hpp"
#include "../include/config.hpp"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static EditorGuiState gState = {};
static const char *gHoverTooltip = nullptr;
static bool gBlockUiInput = false;

static float Clampf(float v, float minV, float maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

static float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

static void SetHoverTooltip(bool hover, const char *text) {
    if (hover && text != nullptr) gHoverTooltip = text;
}

static void AddLog(EditorGuiState &st, const char *message) {
    EngineLogEntry entry = {};
    std::time_t now = std::time(nullptr);
    std::tm tmNow = {};
#if defined(_WIN32)
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    std::strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%d %H:%M:%S", &tmNow);
    entry.text = message ? message : "";
    st.logs.push_back(entry);
    if ((int)st.logs.size() > kUserConfig.maxLogEntries) {
        st.logs.erase(st.logs.begin(), st.logs.begin() + kUserConfig.trimLogEntries);
    }
    std::printf("[%s] %s\n", entry.timestamp, entry.text.c_str());
}

static void UpdateTextFieldBuffer(char *buffer, size_t bufferSize, bool *submit, bool *cancel) {
    int key = GetCharPressed();
    while (key > 0) {
        size_t len = std::strlen(buffer);
        if (key >= 32 && key <= 126 && len + 1 < bufferSize) {
            buffer[len] = (char)key;
            buffer[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        size_t len = std::strlen(buffer);
        if (len > 0) buffer[len - 1] = '\0';
    }

    if (submit) *submit = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    if (cancel) *cancel = IsKeyPressed(KEY_ESCAPE);
}

static const PrimitiveDef *FindPrimitiveDef(PrimitiveKind kind) {
    for (int i = 0; i < kPrimitiveDefCount; i++) {
        const PrimitiveDef &def = kPrimitiveDefs[i];
        if (def.kind == kind) return &def;
    }
    return nullptr;
}

static bool PrimitivePassesFilter(const PrimitiveDef &def, FilterMode mode) {
    if (mode == FilterMode::All) return true;
    if (mode == FilterMode::TwoD) return def.is2D;
    return !def.is2D;
}

static bool DrawButton(Rectangle rect, const char *label, const char *tooltip, bool active = false, int fontSize = 17) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    SetHoverTooltip(hover, tooltip);

    Color fill = (Color){ 56, 61, 71, 255 };
    Color border = (Color){ 96, 103, 118, 255 };
    if (active) {
        fill = (Color){ 84, 104, 142, 255 };
        border = (Color){ 126, 162, 226, 255 };
    } else if (hover) {
        fill = (Color){ 74, 80, 92, 255 };
    }

    DrawRectangleRounded(rect, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(rect, 0.18f, 6, 1.4f, border);

    int tw = MeasureText(label, fontSize);
    DrawText(label,
             (int)(rect.x + rect.width * 0.5f - tw * 0.5f),
             (int)(rect.y + rect.height * 0.5f - fontSize * 0.5f),
             fontSize, RAYWHITE);

    return !gBlockUiInput && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static float DistancePointToSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = Vector2Subtract(b, a);
    float abLenSq = ab.x * ab.x + ab.y * ab.y;
    if (abLenSq <= 0.00001f) return Vector2Distance(p, a);
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / abLenSq;
    t = Clampf(t, 0.0f, 1.0f);
    Vector2 proj = { a.x + ab.x * t, a.y + ab.y * t };
    return Vector2Distance(p, proj);
}

static Vector3 ColorToHSVf(Color c) {
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;
    float maxc = fmaxf(r, fmaxf(g, b));
    float minc = fminf(r, fminf(g, b));
    float d = maxc - minc;

    float h = 0.0f;
    if (d > 0.00001f) {
        if (maxc == r) h = 60.0f * fmodf(((g - b) / d), 6.0f);
        else if (maxc == g) h = 60.0f * (((b - r) / d) + 2.0f);
        else h = 60.0f * (((r - g) / d) + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;

    float s = (maxc <= 0.00001f) ? 0.0f : (d / maxc);
    float v = maxc;
    return { h, s, v };
}

static std::string PrimitiveBaseName(PrimitiveKind kind) {
    const PrimitiveDef *def = FindPrimitiveDef(kind);
    return def ? std::string(def->label) : std::string("Block");
}

static std::string BuildObjectName(PrimitiveKind kind, const std::vector<EditorObject> &objects) {
    std::string base = PrimitiveBaseName(kind);
    int count = 0;
    for (const EditorObject &obj : objects) {
        if (obj.kind == kind) count++;
    }
    char name[64] = { 0 };
    std::snprintf(name, sizeof(name), "%s_%02d", base.c_str(), count + 1);
    return std::string(name);
}

static Vector3 GetSpawnPositionAtTarget(const EditorGuiState &st, PrimitiveKind kind) {
    Vector3 pos = st.orbitTarget;
    const PrimitiveDef *def = FindPrimitiveDef(kind);
    if (def && def->is2D) {
        pos.y = 0.02f;
    } else if (kind == PrimitiveKind::Plane) {
        pos.y = 0.01f;
    } else {
        pos.y = 0.6f;
    }
    return pos;
}

static Vector3 GetMouseWorldOnGround(const EditorGuiState &st) {
    Ray ray = GetMouseRay(GetMousePosition(), st.camera);
    if (fabsf(ray.direction.y) < 0.0001f) return st.orbitTarget;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) t = 0.0f;
    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    hit.y = 0.0f;
    return hit;
}

static bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3 *hitOut) {
    float denom = Vector3DotProduct(ray.direction, planeNormal);
    if (fabsf(denom) < 0.00001f) return false;

    float t = Vector3DotProduct(Vector3Subtract(planePoint, ray.position), planeNormal) / denom;
    if (t < 0.0f) return false;

    if (hitOut) {
        *hitOut = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    }
    return true;
}

static void SpawnObject(EditorGuiState &st, PrimitiveKind kind, Vector3 pos, bool selectNew) {
    const PrimitiveDef *def = FindPrimitiveDef(kind);
    if (!def) return;

    EditorObject obj = {};
    obj.name = BuildObjectName(kind, st.objects);
    obj.kind = kind;
    obj.is2D = def->is2D;
    obj.visible = true;
    obj.anchored = false;
    obj.position = pos;
    obj.rotation = { 0.0f, 0.0f, 0.0f };
    obj.scale = { 1.0f, 1.0f, 1.0f };

    if (def->is2D) {
        obj.position.y = 0.02f;
        obj.scale = { 1.2f, 1.0f, 1.2f };
    } else if (kind == PrimitiveKind::Plane) {
        obj.position.y = 0.01f;
        obj.scale = { 1.8f, 1.0f, 1.8f };
    }

    float hue = (float)GetRandomValue(0, 359);
    obj.color = ColorFromHSV(hue, 0.72f, 0.95f);
    st.objects.push_back(obj);
    char logMsg[160] = { 0 };
    std::snprintf(logMsg, sizeof(logMsg), "Created block '%s' (%s)", obj.name.c_str(), def->label);
    AddLog(st, logMsg);

    if (selectNew) {
        st.selectedIndex = (int)st.objects.size() - 1;
        st.colorSyncIndex = -1;
    }
}

static void RequestDeleteObject(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.objects.size()) return;
    st.showDeleteConfirm = true;
    st.deleteTargetIndex = index;
    std::snprintf(st.deleteTargetName, sizeof(st.deleteTargetName), "%s", st.objects[index].name.c_str());
}

static void DeleteObjectAtIndex(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.objects.size()) return;

    std::string deletedName = st.objects[index].name;
    st.objects.erase(st.objects.begin() + index);

    if (st.selectedIndex == index) {
        st.selectedIndex = -1;
    } else if (st.selectedIndex > index) {
        st.selectedIndex--;
    }

    if (st.renameIndex == index) {
        st.renameIndex = -1;
        st.renameFromInspector = false;
    } else if (st.renameIndex > index) {
        st.renameIndex--;
    }

    st.colorSyncIndex = -1;

    char logMsg[160] = { 0 };
    std::snprintf(logMsg, sizeof(logMsg), "Deleted block '%s'", deletedName.c_str());
    AddLog(st, logMsg);
}

static float ApproxObjectRadius(const EditorObject &obj) {
    float scaleLen = Vector3Length(obj.scale);
    if (scaleLen < 0.2f) scaleLen = 0.2f;
    return scaleLen * (obj.is2D ? 0.55f : 0.85f);
}

static int PickObject3D(const EditorGuiState &st, const Rectangle &viewport) {
    if (!CheckCollisionPointRec(GetMousePosition(), viewport)) return -1;

    Ray ray = GetMouseRay(GetMousePosition(), st.camera);
    float bestDist = 1e30f;
    int bestIdx = -1;
    for (int i = 0; i < (int)st.objects.size(); i++) {
        const EditorObject &obj = st.objects[i];
        if (!obj.visible) continue;
        if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;

        RayCollision hit = GetRayCollisionSphere(ray, obj.position, ApproxObjectRadius(obj));
        if (hit.hit && hit.distance < bestDist) {
            bestDist = hit.distance;
            bestIdx = i;
        }
    }
    return bestIdx;
}

static void UpdateCameraFromOrbit(EditorGuiState &st) {
    st.orbitPitch = Clampf(st.orbitPitch, -1.45f, 1.45f);
    st.orbitDistance = Clampf(st.orbitDistance, 3.0f, 220.0f);

    Vector3 forward = {
        cosf(st.orbitPitch) * sinf(st.orbitYaw),
        sinf(st.orbitPitch),
        cosf(st.orbitPitch) * cosf(st.orbitYaw)
    };
    st.camera.target = st.orbitTarget;
    st.camera.position = Vector3Add(st.orbitTarget, Vector3Scale(forward, st.orbitDistance));
    st.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

static void UpdatePreviewCameraDirection(EditorGuiState &st) {
    st.previewPitch = Clampf(st.previewPitch, -1.53f, 1.53f);
    Vector3 forward = {
        cosf(st.previewPitch) * sinf(st.previewYaw),
        sinf(st.previewPitch),
        cosf(st.previewPitch) * cosf(st.previewYaw)
    };
    st.previewCamera.target = Vector3Add(st.previewCamera.position, forward);
    st.previewCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

static void OpenPreview(EditorGuiState &st) {
    Vector3 look = Vector3Normalize(Vector3Subtract(st.camera.target, st.camera.position));
    if (Vector3LengthSqr(look) < 0.00001f) look = (Vector3){ 0.0f, 0.0f, -1.0f };

    st.previewOpen = true;
    st.previewCamera = {};
    st.previewCamera.position = st.camera.position;
    st.previewCamera.fovy = 60.0f;
    st.previewCamera.projection = CAMERA_PERSPECTIVE;
    st.previewYaw = atan2f(look.x, look.z);
    st.previewPitch = asinf(Clampf(look.y, -1.0f, 1.0f));
    UpdatePreviewCameraDirection(st);
    AddLog(st, "Preview opened");
}

static Vector3 AxisVectorFromGizmoAxis(GizmoAxis axis) {
    if (axis == GizmoAxis::X) return (Vector3){ 1.0f, 0.0f, 0.0f };
    if (axis == GizmoAxis::Y) return (Vector3){ 0.0f, 1.0f, 0.0f };
    if (axis == GizmoAxis::Z) return (Vector3){ 0.0f, 0.0f, 1.0f };
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

static void DrawObject3D(const EditorObject &obj) {
    rlPushMatrix();
    rlTranslatef(obj.position.x, obj.position.y, obj.position.z);
    rlRotatef(obj.rotation.x, 1, 0, 0);
    rlRotatef(obj.rotation.y, 0, 1, 0);
    rlRotatef(obj.rotation.z, 0, 0, 1);
    rlScalef(obj.scale.x, obj.scale.y, obj.scale.z);

    Color wire = ColorAlpha(BLACK, 0.72f);
    switch (obj.kind) {
        case PrimitiveKind::Cube:
            DrawCubeV((Vector3){ 0, 0, 0 }, (Vector3){ 1, 1, 1 }, obj.color);
            DrawCubeWiresV((Vector3){ 0, 0, 0 }, (Vector3){ 1, 1, 1 }, wire);
            break;
        case PrimitiveKind::Sphere:
            DrawSphere((Vector3){ 0, 0, 0 }, 0.58f, obj.color);
            DrawSphereWires((Vector3){ 0, 0, 0 }, 0.58f, 14, 12, wire);
            break;
        case PrimitiveKind::Cylinder:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.45f, 0.45f, 1.2f, 24, obj.color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.45f, 0.45f, 1.2f, 24, wire);
            break;
        case PrimitiveKind::Cone:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.0f, 0.58f, 1.2f, 24, obj.color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.0f, 0.58f, 1.2f, 24, wire);
            break;
        case PrimitiveKind::Plane:
            DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 1.6f, 1.6f }, obj.color);
            DrawLine3D((Vector3){ -0.8f, 0.01f, -0.8f }, (Vector3){ 0.8f, 0.01f, -0.8f }, wire);
            DrawLine3D((Vector3){ 0.8f, 0.01f, -0.8f }, (Vector3){ 0.8f, 0.01f, 0.8f }, wire);
            DrawLine3D((Vector3){ 0.8f, 0.01f, 0.8f }, (Vector3){ -0.8f, 0.01f, 0.8f }, wire);
            DrawLine3D((Vector3){ -0.8f, 0.01f, 0.8f }, (Vector3){ -0.8f, 0.01f, -0.8f }, wire);
            break;
        case PrimitiveKind::Rectangle2D:
            DrawCubeV((Vector3){ 0, 0, 0 }, (Vector3){ 1.25f, 0.04f, 0.9f }, obj.color);
            DrawCubeWiresV((Vector3){ 0, 0, 0 }, (Vector3){ 1.25f, 0.04f, 0.9f }, wire);
            break;
        case PrimitiveKind::Circle2D:
            DrawCylinder((Vector3){ 0, 0, 0 }, 0.55f, 0.55f, 0.04f, 32, obj.color);
            DrawCylinderWires((Vector3){ 0, 0, 0 }, 0.55f, 0.55f, 0.04f, 32, wire);
            break;
        case PrimitiveKind::Triangle2D:
            DrawTriangle3D((Vector3){ -0.6f, 0.02f, 0.45f },
                           (Vector3){ 0.65f, 0.02f, 0.45f },
                           (Vector3){ 0.0f, 0.02f, -0.55f }, obj.color);
            break;
        case PrimitiveKind::Ring2D:
            DrawCircle3D((Vector3){ 0, 0.04f, 0 }, 0.68f, (Vector3){ 1, 0, 0 }, 90.0f, obj.color);
            DrawCircle3D((Vector3){ 0, 0.04f, 0 }, 0.42f, (Vector3){ 1, 0, 0 }, 90.0f, (Color){ 22, 25, 31, 255 });
            break;
        case PrimitiveKind::Poly2D: {
            const int sides = 6;
            const float r = 0.64f;
            for (int i = 0; i < sides; i++) {
                float a0 = (float)i / (float)sides * 2.0f * PI;
                float a1 = (float)(i + 1) / (float)sides * 2.0f * PI;
                DrawLine3D((Vector3){ cosf(a0) * r, 0.03f, sinf(a0) * r },
                           (Vector3){ cosf(a1) * r, 0.03f, sinf(a1) * r },
                           obj.color);
            }
            break;
        }
        case PrimitiveKind::Line2D:
            DrawLine3D((Vector3){ -0.65f, 0.03f, 0.0f }, (Vector3){ 0.65f, 0.03f, 0.0f }, obj.color);
            break;
    }

    rlPopMatrix();
}

static void DrawSelectionHighlight(const EditorObject &obj) {
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f);
    Color c = ColorAlpha((Color){ 255, 235, 120, 255 }, 0.28f + pulse * 0.42f);
    DrawSphereWires(obj.position, ApproxObjectRadius(obj) * 1.35f, 14, 12, c);
}

static void DrawGizmoVisual(const EditorObject &obj, GizmoMode mode, GizmoAxis activeAxis) {
    float len = 2.1f;
    Vector3 p = obj.position;
    Color xC = (activeAxis == GizmoAxis::X) ? (Color){ 255, 140, 140, 255 } : (Color){ 240, 82, 82, 255 };
    Color yC = (activeAxis == GizmoAxis::Y) ? (Color){ 166, 255, 166, 255 } : (Color){ 92, 217, 108, 255 };
    Color zC = (activeAxis == GizmoAxis::Z) ? (Color){ 156, 198, 255, 255 } : (Color){ 88, 146, 245, 255 };

    DrawLine3D(p, Vector3Add(p, (Vector3){ len, 0, 0 }), xC);
    DrawLine3D(p, Vector3Add(p, (Vector3){ 0, len, 0 }), yC);
    DrawLine3D(p, Vector3Add(p, (Vector3){ 0, 0, len }), zC);

    float hSize = (mode == GizmoMode::Move) ? 0.14f : (mode == GizmoMode::Rotate ? 0.12f : 0.14f);
    DrawSphere(Vector3Add(p, (Vector3){ len, 0, 0 }), hSize, xC);
    DrawSphere(Vector3Add(p, (Vector3){ 0, len, 0 }), hSize, yC);
    DrawSphere(Vector3Add(p, (Vector3){ 0, 0, len }), hSize, zC);

    if (mode == GizmoMode::Rotate) {
        DrawCircle3D(p, 1.2f, (Vector3){ 1, 0, 0 }, 90.0f, xC);
        DrawCircle3D(p, 1.2f, (Vector3){ 0, 1, 0 }, 90.0f, yC);
        DrawCircle3D(p, 1.2f, (Vector3){ 0, 0, 1 }, 90.0f, zC);
    }
}

static GizmoAxis DetectGizmoAxis(const EditorGuiState &st, const Rectangle &viewport, const EditorObject &obj) {
    if (!CheckCollisionPointRec(GetMousePosition(), viewport)) return GizmoAxis::None;
    Vector2 mouse = GetMousePosition();
    Vector3 p = obj.position;
    float len = 2.1f;

    Vector2 s = GetWorldToScreen(p, st.camera);
    Vector2 x = GetWorldToScreen(Vector3Add(p, (Vector3){ len, 0, 0 }), st.camera);
    Vector2 y = GetWorldToScreen(Vector3Add(p, (Vector3){ 0, len, 0 }), st.camera);
    Vector2 z = GetWorldToScreen(Vector3Add(p, (Vector3){ 0, 0, len }), st.camera);

    float dx = DistancePointToSegment(mouse, s, x);
    float dy = DistancePointToSegment(mouse, s, y);
    float dz = DistancePointToSegment(mouse, s, z);

    float best = 18.0f;
    GizmoAxis axis = GizmoAxis::None;
    if (dx < best) { best = dx; axis = GizmoAxis::X; }
    if (dy < best) { best = dy; axis = GizmoAxis::Y; }
    if (dz < best) { best = dz; axis = GizmoAxis::Z; }
    return axis;
}

static void ApplyGizmoDrag(EditorObject &obj, GizmoMode mode, GizmoAxis axis, Vector2 delta, float rotateSensitivity, float scaleSensitivity) {
    float d = (delta.x - delta.y) * 0.02f;
    if (axis == GizmoAxis::X) {
        if (mode == GizmoMode::Rotate) obj.rotation.x += d * rotateSensitivity;
        else if (mode == GizmoMode::Scale) obj.scale.x = Clampf(obj.scale.x + d * scaleSensitivity, 0.1f, 100.0f);
    } else if (axis == GizmoAxis::Y) {
        if (mode == GizmoMode::Rotate) obj.rotation.y += d * rotateSensitivity;
        else if (mode == GizmoMode::Scale) obj.scale.y = Clampf(obj.scale.y + d * scaleSensitivity, 0.1f, 100.0f);
    } else if (axis == GizmoAxis::Z) {
        if (mode == GizmoMode::Rotate) obj.rotation.z += d * rotateSensitivity;
        else if (mode == GizmoMode::Scale) obj.scale.z = Clampf(obj.scale.z + d * scaleSensitivity, 0.1f, 100.0f);
    }
}

static void ApplyMoveAxisDrag(EditorGuiState &st, EditorObject &obj, GizmoAxis axis, Vector2 mouseDelta) {
    Vector3 axisVec = AxisVectorFromGizmoAxis(axis);
    if (Vector3LengthSqr(axisVec) < 0.5f) return;

    Vector2 p0 = GetWorldToScreen(obj.position, st.camera);
    Vector2 p1 = GetWorldToScreen(Vector3Add(obj.position, axisVec), st.camera);
    Vector2 screenAxis = Vector2Subtract(p1, p0);
    float screenLen = Vector2Length(screenAxis);
    if (screenLen < 0.0001f) return;

    Vector2 dir = Vector2Scale(screenAxis, 1.0f / screenLen);
    float pixels = Vector2DotProduct(mouseDelta, dir);
    float dist = Vector3Distance(st.camera.position, obj.position);
    float worldPerPixel = (0.0022f * dist) * st.gizmoMoveSensitivity;
    float worldDelta = pixels * worldPerPixel;

    obj.position = Vector3Add(obj.position, Vector3Scale(axisVec, worldDelta));
}

static void DrawPrimitivePreview(Rectangle rect, PrimitiveKind kind, Color tint) {
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f - 8.0f;
    switch (kind) {
        case PrimitiveKind::Cube:
            DrawRectangleRounded((Rectangle){ cx - 16, cy - 16, 32, 32 }, 0.15f, 5, tint);
            DrawRectangleLinesEx((Rectangle){ cx - 16, cy - 16, 32, 32 }, 1.3f, BLACK);
            break;
        case PrimitiveKind::Sphere:
            DrawCircle((int)cx, (int)cy, 16.0f, tint);
            DrawCircleLines((int)cx, (int)cy, 16.0f, BLACK);
            break;
        case PrimitiveKind::Cylinder:
            DrawRectangleRounded((Rectangle){ cx - 12, cy - 18, 24, 36 }, 0.5f, 6, tint);
            DrawRectangleLinesEx((Rectangle){ cx - 12, cy - 18, 24, 36 }, 1.2f, BLACK);
            break;
        case PrimitiveKind::Cone:
            DrawTriangle((Vector2){ cx, cy - 18 }, (Vector2){ cx - 18, cy + 16 }, (Vector2){ cx + 18, cy + 16 }, tint);
            DrawTriangleLines((Vector2){ cx, cy - 18 }, (Vector2){ cx - 18, cy + 16 }, (Vector2){ cx + 18, cy + 16 }, BLACK);
            break;
        case PrimitiveKind::Plane:
            DrawRectangle((int)cx - 18, (int)cy - 10, 36, 20, tint);
            DrawRectangleLines((int)cx - 18, (int)cy - 10, 36, 20, BLACK);
            break;
        case PrimitiveKind::Rectangle2D:
            DrawRectangleRounded((Rectangle){ cx - 18, cy - 12, 36, 24 }, 0.15f, 6, tint);
            DrawRectangleLinesEx((Rectangle){ cx - 18, cy - 12, 36, 24 }, 1.2f, BLACK);
            break;
        case PrimitiveKind::Circle2D:
            DrawCircle((int)cx, (int)cy, 14.0f, tint);
            DrawCircleLines((int)cx, (int)cy, 14.0f, BLACK);
            break;
        case PrimitiveKind::Triangle2D:
            DrawTriangle((Vector2){ cx, cy - 15 }, (Vector2){ cx - 16, cy + 13 }, (Vector2){ cx + 16, cy + 13 }, tint);
            DrawTriangleLines((Vector2){ cx, cy - 15 }, (Vector2){ cx - 16, cy + 13 }, (Vector2){ cx + 16, cy + 13 }, BLACK);
            break;
        case PrimitiveKind::Ring2D:
            DrawRing((Vector2){ cx, cy }, 8.0f, 16.0f, 0, 360, 48, tint);
            DrawCircleLines((int)cx, (int)cy, 16.0f, BLACK);
            break;
        case PrimitiveKind::Poly2D:
            DrawPoly((Vector2){ cx, cy }, 6, 16.0f, 0.0f, tint);
            DrawPolyLines((Vector2){ cx, cy }, 6, 16.0f, 0.0f, BLACK);
            break;
        case PrimitiveKind::Line2D:
            DrawLineEx((Vector2){ cx - 18, cy }, (Vector2){ cx + 18, cy }, 3.0f, tint);
            break;
    }
}

static void DrawTooltip() {
    if (!gHoverTooltip) return;
    Vector2 mouse = GetMousePosition();
    int fs = 17;
    int tw = MeasureText(gHoverTooltip, fs);
    Rectangle tip = { mouse.x + 14.0f, mouse.y + 14.0f, (float)tw + 14.0f, 30.0f };
    if (tip.x + tip.width > GetScreenWidth()) tip.x = GetScreenWidth() - tip.width - 4.0f;
    if (tip.y + tip.height > GetScreenHeight()) tip.y = mouse.y - tip.height - 8.0f;
    DrawRectangleRounded(tip, 0.16f, 6, (Color){ 15, 18, 23, 245 });
    DrawRectangleRoundedLinesEx(tip, 0.16f, 6, 1.2f, (Color){ 102, 112, 131, 255 });
    DrawText(gHoverTooltip, (int)tip.x + 7, (int)tip.y + 6, fs, (Color){ 226, 231, 240, 255 });
}

static void Draw2DViewportObjects(const Rectangle &viewport, const EditorGuiState &st) {
    float scale = 24.0f;
    Vector2 center = { viewport.x + viewport.width * 0.5f, viewport.y + viewport.height * 0.5f };

    for (int i = 0; i < (int)st.objects.size(); i++) {
        const EditorObject &obj = st.objects[i];
        if (!obj.visible) continue;
        if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;

        Vector2 p = { center.x + obj.position.x * scale, center.y + obj.position.z * scale };
        float sz = 10.0f + Vector3Length(obj.scale) * 3.0f;
        DrawCircleV(p, sz, obj.color);
        DrawCircleLines((int)p.x, (int)p.y, sz, BLACK);

        if (i == st.selectedIndex) {
            DrawCircleLines((int)p.x, (int)p.y, sz + 5.0f, (Color){ 255, 236, 130, 255 });
        }
    }
}

static void Draw2DGrid(const Rectangle &viewport) {
    BeginScissorMode((int)viewport.x + 1, (int)viewport.y + 1, (int)viewport.width - 2, (int)viewport.height - 2);
    DrawRectangleRec(viewport, (Color){ 24, 27, 33, 255 });

    const float spacing = 24.0f;
    int linesX = (int)(viewport.width / spacing);
    int linesY = (int)(viewport.height / spacing);
    for (int i = 0; i <= linesX; i++) {
        float x = viewport.x + i * spacing;
        bool major = (i % 5) == 0;
        DrawLineEx((Vector2){ x, viewport.y }, (Vector2){ x, viewport.y + viewport.height },
                   1.0f, major ? (Color){ 66, 74, 89, 255 } : (Color){ 44, 49, 58, 255 });
    }
    for (int i = 0; i <= linesY; i++) {
        float y = viewport.y + i * spacing;
        bool major = (i % 5) == 0;
        DrawLineEx((Vector2){ viewport.x, y }, (Vector2){ viewport.x + viewport.width, y },
                   1.0f, major ? (Color){ 66, 74, 89, 255 } : (Color){ 44, 49, 58, 255 });
    }

    float cx = viewport.x + viewport.width * 0.5f;
    float cy = viewport.y + viewport.height * 0.5f;
    DrawLineEx((Vector2){ cx, viewport.y }, (Vector2){ cx, viewport.y + viewport.height }, 2.0f, (Color){ 98, 176, 108, 255 });
    DrawLineEx((Vector2){ viewport.x, cy }, (Vector2){ viewport.x + viewport.width, cy }, 2.0f, (Color){ 214, 93, 93, 255 });
    EndScissorMode();

    DrawRectangleLinesEx(viewport, 2.0f, (Color){ 90, 98, 114, 255 });
    DrawText("2D Viewport", (int)viewport.x + 14, (int)viewport.y + 10, 20, (Color){ 214, 220, 232, 255 });
}

static void Draw3DViewport(const Rectangle &viewport, EditorGuiState &st) {
    BeginScissorMode((int)viewport.x + 1, (int)viewport.y + 1, (int)viewport.width - 2, (int)viewport.height - 2);
    DrawRectangleRec(viewport, (Color){ 23, 26, 32, 255 });

    BeginMode3D(st.camera);
        DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 240, 240 }, (Color){ 24, 31, 35, 255 });
        DrawGrid(160, 1.0f);

        for (int i = 0; i < (int)st.objects.size(); i++) {
            const EditorObject &obj = st.objects[i];
            if (!obj.visible) continue;
            if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;
            DrawObject3D(obj);
        }

        if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
            const EditorObject &obj = st.objects[st.selectedIndex];
            if (obj.visible) {
                DrawSelectionHighlight(obj);
                if (!obj.anchored) {
                    DrawGizmoVisual(obj, st.gizmoMode, st.activeAxis);
                }
            }
        }
    EndMode3D();

    EndScissorMode();

    DrawRectangleLinesEx(viewport, 2.0f, (Color){ 90, 98, 114, 255 });
    DrawText("3D Viewport", (int)viewport.x + 14, (int)viewport.y + 10, 20, (Color){ 214, 220, 232, 255 });
}

static void DrawPreviewWindow(EditorGuiState &st, float dt) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    //close preview quickly with keyboard
    if (IsKeyPressed(KEY_ESCAPE)) {
        st.previewOpen = false;
        AddLog(st, "Preview closed");
        return;
    }

    Vector3 lookDir = Vector3Normalize(Vector3Subtract(st.previewCamera.target, st.previewCamera.position));
    if (Vector3LengthSqr(lookDir) < 0.00001f) lookDir = (Vector3){ 0.0f, 0.0f, -1.0f };
    Vector3 flatForward = Vector3Normalize((Vector3){ lookDir.x, 0.0f, lookDir.z });
    if (Vector3LengthSqr(flatForward) < 0.00001f) flatForward = (Vector3){ 0.0f, 0.0f, -1.0f };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(lookDir, (Vector3){ 0.0f, 1.0f, 0.0f }));
    if (Vector3LengthSqr(right) < 0.00001f) right = (Vector3){ 1.0f, 0.0f, 0.0f };
    Vector3 up = (Vector3){ 0.0f, 1.0f, 0.0f };

    //mouse look + pan controls in preview window
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        Vector2 d = GetMouseDelta();
        st.previewYaw += d.x * st.previewLookSensitivity;
        st.previewPitch -= d.y * st.previewLookSensitivity;
    }
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        float pan = st.previewMoveSpeed * st.previewPanSensitivity * dt;
        st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(right, -d.x * pan));
        st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(up, d.y * pan));
    }

    //keyboard rotate controls for preview
    float keyLook = st.previewLookSensitivity * 200.0f * dt;
    if (IsKeyDown(KEY_LEFT)) st.previewYaw -= keyLook;
    if (IsKeyDown(KEY_RIGHT)) st.previewYaw += keyLook;
    if (IsKeyDown(KEY_UP)) st.previewPitch += keyLook;
    if (IsKeyDown(KEY_DOWN)) st.previewPitch -= keyLook;

    //keyboard move controls for preview
    float speedMul = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 2.5f : 1.0f;
    float move = st.previewMoveSpeed * speedMul * dt;
    if (IsKeyDown(KEY_W)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(flatForward, move));
    if (IsKeyDown(KEY_S)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(flatForward, -move));
    if (IsKeyDown(KEY_A)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(right, -move));
    if (IsKeyDown(KEY_D)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(right, move));

    //mouse wheel zoom in/out following view direction
    float wheel = GetMouseWheelMove();
    if (fabsf(wheel) > 0.0001f) {
        st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(lookDir, wheel * st.previewZoomSpeed));
    }

    UpdatePreviewCameraDirection(st);

    DrawRectangle(0, 0, sw, sh, BLACK);

    BeginMode3D(st.previewCamera);
        DrawPlane((Vector3){ 0, -0.01f, 0 }, (Vector2){ 280.0f, 280.0f }, (Color){ 8, 8, 8, 255 });
        DrawGrid(180, 1.0f);

        for (int i = 0; i < (int)st.objects.size(); i++) {
            const EditorObject &obj = st.objects[i];
            if (!obj.visible) continue;
            if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;
            DrawObject3D(obj);
        }
    EndMode3D();

    DrawRectangle(0, 0, sw, 40, (Color){ 15, 18, 24, 230 });
    DrawLine(0, 40, sw, 40, (Color){ 80, 88, 103, 255 });

    DrawText("Preview Window", 12, 10, 21, RAYWHITE);
    DrawText("WASD move | Arrow keys rotate | RMB look | MMB pan | Mouse wheel zoom | Shift speed", 196, 12, 16, (Color){ 190, 199, 214, 255 });
    if (DrawButton((Rectangle){ (float)sw - 112.0f, 6.0f, 98.0f, 28.0f }, "Close", "Close preview and return")) {
        st.previewOpen = false;
        AddLog(st, "Preview closed");
    }

    if (st.objects.empty()) {
        DrawText("No blocks in scene.", 18, 52, 20, (Color){ 180, 188, 203, 255 });
    }
}

static bool DrawFloatStepper(const char *label, float *value, float step, float minV, float maxV, float x, float y, float width, const char *tooltip) {
    DrawText(label, (int)x, (int)y + 4, 17, (Color){ 204, 210, 223, 255 });
    Rectangle minusRect = { x + 92.0f, y, 26.0f, 24.0f };
    Rectangle plusRect = { x + width - 26.0f, y, 26.0f, 24.0f };
    Rectangle valueRect = { x + 122.0f, y, width - 152.0f, 24.0f };
    bool changed = false;
    if (DrawButton(minusRect, "-", tooltip)) {
        *value = Clampf(*value - step, minV, maxV);
        changed = true;
    }
    DrawRectangleRounded(valueRect, 0.14f, 6, (Color){ 35, 40, 49, 255 });
    DrawRectangleRoundedLinesEx(valueRect, 0.14f, 6, 1.0f, (Color){ 81, 89, 104, 255 });
    char txt[64] = { 0 };
    std::snprintf(txt, sizeof(txt), "%.2f", *value);
    int tw = MeasureText(txt, 16);
    DrawText(txt, (int)(valueRect.x + valueRect.width * 0.5f - tw * 0.5f), (int)y + 4, 16, RAYWHITE);
    if (DrawButton(plusRect, "+", tooltip)) {
        *value = Clampf(*value + step, minV, maxV);
        changed = true;
    }
    return changed;
}

static void DrawSettingsPopup(Rectangle rect, EditorGuiState &st) {
    DrawRectangleRounded(rect, 0.05f, 8, (Color){ 32, 36, 44, 248 });
    DrawRectangleRoundedLinesEx(rect, 0.05f, 8, 1.2f, (Color){ 95, 105, 123, 255 });
    DrawText("Settings", (int)rect.x + 12, (int)rect.y + 8, 20, (Color){ 227, 233, 241, 255 });

    float x = rect.x + 12.0f;
    float y = rect.y + 36.0f;
    float w = rect.width - 24.0f;

    DrawFloatStepper("Orbit", &st.cameraOrbitSensitivity, 0.0005f, 0.001f, 0.030f, x, y, w, "Camera orbit sensitivity");
    y += 30.0f;
    DrawFloatStepper("Grid Pan", &st.cameraPanSensitivity, 0.0005f, 0.001f, 0.040f, x, y, w, "Grid drag pan sensitivity");
    y += 30.0f;
    DrawFloatStepper("Grid Zoom", &st.cameraZoomSensitivity, 0.005f, 0.010f, 0.200f, x, y, w, "Grid mouse wheel zoom sensitivity");
    y += 30.0f;
    DrawFloatStepper("Move", &st.gizmoMoveSensitivity, 0.05f, 0.10f, 4.0f, x, y, w, "Move gizmo sensitivity");
    y += 30.0f;
    DrawFloatStepper("Rotate", &st.gizmoRotateSensitivity, 1.0f, 4.0f, 120.0f, x, y, w, "Rotate gizmo sensitivity");
    y += 30.0f;
    DrawFloatStepper("Scale", &st.gizmoScaleSensitivity, 0.05f, 0.05f, 2.0f, x, y, w, "Scale gizmo sensitivity");
    y += 30.0f;
    DrawFloatStepper("Pv Move", &st.previewMoveSpeed, 0.5f, 1.0f, 60.0f, x, y, w, "Preview camera movement speed");
    y += 30.0f;
    DrawFloatStepper("Pv Look", &st.previewLookSensitivity, 0.0005f, 0.0005f, 0.03f, x, y, w, "Preview mouse/arrow look sensitivity");
    y += 30.0f;
    DrawFloatStepper("Pv Zoom", &st.previewZoomSpeed, 0.2f, 0.2f, 30.0f, x, y, w, "Preview wheel zoom speed");
    y += 30.0f;
    DrawFloatStepper("Pv Pan", &st.previewPanSensitivity, 0.1f, 0.3f, 8.0f, x, y, w, "Preview middle-mouse pan sensitivity");
}

static void DrawDeleteConfirmPopup(EditorGuiState &st) {
    if (!st.showDeleteConfirm) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        st.showDeleteConfirm = false;
        return;
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 140 });

    Rectangle box = { sw * 0.5f - 210.0f, sh * 0.5f - 90.0f, 420.0f, 180.0f };
    DrawRectangleRounded(box, 0.08f, 8, (Color){ 34, 38, 46, 255 });
    DrawRectangleRoundedLinesEx(box, 0.08f, 8, 1.4f, (Color){ 98, 108, 126, 255 });

    DrawText("Confirm Deletion", (int)box.x + 14, (int)box.y + 12, 24, (Color){ 232, 236, 243, 255 });

    char msg[192] = { 0 };
    std::snprintf(msg, sizeof(msg), "Delete block '%s'?", st.deleteTargetName);
    DrawText(msg, (int)box.x + 14, (int)box.y + 56, 20, (Color){ 205, 212, 224, 255 });
    DrawText("This action cannot be undone.", (int)box.x + 14, (int)box.y + 84, 17, (Color){ 167, 175, 190, 255 });

    Rectangle cancelBtn = { box.x + 14.0f, box.y + box.height - 44.0f, 130.0f, 30.0f };
    Rectangle deleteBtn = { box.x + box.width - 144.0f, box.y + box.height - 44.0f, 130.0f, 30.0f };
    bool prevBlock = gBlockUiInput;
    gBlockUiInput = false;
    if (DrawButton(cancelBtn, "Cancel", "Cancel delete")) {
        st.showDeleteConfirm = false;
    }
    if (DrawButton(deleteBtn, "Delete", "Confirm delete")) {
        DeleteObjectAtIndex(st, st.deleteTargetIndex);
        st.showDeleteConfirm = false;
    }
    gBlockUiInput = prevBlock;
}

static void DrawInspectorPanel(Rectangle panelRect, EditorGuiState &st) {
    DrawRectangleRec(panelRect, (Color){ 37, 41, 49, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 82, 88, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 36, (Color){ 44, 49, 58, 255 });
    DrawText("Hierarchy / Inspector", (int)panelRect.x + 12, (int)panelRect.y + 8, 20, (Color){ 225, 230, 238, 255 });

    if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
        Rectangle deleteBtn = { panelRect.x + panelRect.width - 34.0f, panelRect.y + 6.0f, 24.0f, 24.0f };
        if (DrawButton(deleteBtn, "X", "Delete selected block", false, 15)) {
            RequestDeleteObject(st, st.selectedIndex);
        }
    }

    if (st.selectedIndex < 0 || st.selectedIndex >= (int)st.objects.size()) {
        DrawText("No block selected.", (int)panelRect.x + 16, (int)panelRect.y + 54, 18, (Color){ 188, 194, 206, 255 });
        return;
    }

    EditorObject &obj = st.objects[st.selectedIndex];
    float x = panelRect.x + 14.0f;
    float y = panelRect.y + 50.0f;
    float w = panelRect.width - 28.0f;

    DrawText("Name:", (int)x, (int)y, 18, (Color){ 198, 206, 219, 255 });
    Rectangle renameRect = { x + 64.0f, y - 2.0f, w - 64.0f, 24.0f };
    if (st.renameIndex == st.selectedIndex && st.renameFromInspector) {
        DrawRectangleRounded(renameRect, 0.12f, 6, (Color){ 33, 39, 47, 255 });
        DrawRectangleRoundedLinesEx(renameRect, 0.12f, 6, 1.0f, (Color){ 124, 157, 230, 255 });
        DrawText(st.renameBuffer, (int)renameRect.x + 6, (int)renameRect.y + 4, 16, RAYWHITE);
        bool submit = false;
        bool cancel = false;
        UpdateTextFieldBuffer(st.renameBuffer, sizeof(st.renameBuffer), &submit, &cancel);
        if (submit) {
            if (std::strlen(st.renameBuffer) > 0) {
                char oldName[64] = { 0 };
                std::snprintf(oldName, sizeof(oldName), "%s", obj.name.c_str());
                obj.name = st.renameBuffer;
                char logMsg[196] = { 0 };
                std::snprintf(logMsg, sizeof(logMsg), "Renamed block '%s' -> '%s'", oldName, obj.name.c_str());
                AddLog(st, logMsg);
            }
            st.renameIndex = -1;
            st.renameFromInspector = false;
        }
        if (cancel) {
            st.renameIndex = -1;
            st.renameFromInspector = false;
        }
    } else {
        if (DrawButton(renameRect, obj.name.c_str(), "Click to rename")) {
            st.renameIndex = st.selectedIndex;
            st.renameFromInspector = true;
            std::snprintf(st.renameBuffer, sizeof(st.renameBuffer), "%s", obj.name.c_str());
        }
    }

    y += 32.0f;
    const PrimitiveDef *def = FindPrimitiveDef(obj.kind);
    DrawText(TextFormat("Type: %s", def ? def->label : "Unknown"), (int)x, (int)y, 18, (Color){ 198, 206, 219, 255 });

    y += 30.0f;
    Rectangle visBtn = { x, y, 122, 26 };
    Rectangle ancBtn = { x + 132, y, 122, 26 };
    if (DrawButton(visBtn, obj.visible ? "Visible" : "Hidden", "Toggle object visibility", obj.visible)) obj.visible = !obj.visible;
    if (DrawButton(ancBtn, obj.anchored ? "Anchored" : "Free", "Toggle anchor (locked/unlocked movement)", obj.anchored)) obj.anchored = !obj.anchored;

    y += 36.0f;
    DrawText("Transform", (int)x, (int)y, 20, (Color){ 220, 227, 238, 255 });
    y += 28.0f;

    DrawFloatStepper("Pos X", &obj.position.x, 0.10f, -9999.0f, 9999.0f, x, y, w, "Adjust X position");
    y += 30.0f;
    DrawFloatStepper("Pos Y", &obj.position.y, 0.10f, -9999.0f, 9999.0f, x, y, w, "Adjust Y position");
    y += 30.0f;
    DrawFloatStepper("Pos Z", &obj.position.z, 0.10f, -9999.0f, 9999.0f, x, y, w, "Adjust Z position");
    y += 34.0f;

    DrawFloatStepper("Rot X", &obj.rotation.x, 1.0f, -36000.0f, 36000.0f, x, y, w, "Adjust X rotation");
    y += 30.0f;
    DrawFloatStepper("Rot Y", &obj.rotation.y, 1.0f, -36000.0f, 36000.0f, x, y, w, "Adjust Y rotation");
    y += 30.0f;
    DrawFloatStepper("Rot Z", &obj.rotation.z, 1.0f, -36000.0f, 36000.0f, x, y, w, "Adjust Z rotation");
    y += 34.0f;

    DrawFloatStepper("Scl X", &obj.scale.x, 0.05f, 0.05f, 9999.0f, x, y, w, "Adjust X scale");
    y += 30.0f;
    DrawFloatStepper("Scl Y", &obj.scale.y, 0.05f, 0.05f, 9999.0f, x, y, w, "Adjust Y scale");
    y += 30.0f;
    DrawFloatStepper("Scl Z", &obj.scale.z, 0.05f, 0.05f, 9999.0f, x, y, w, "Adjust Z scale");

    y += 38.0f;
    DrawText("Color", (int)x, (int)y, 19, (Color){ 214, 220, 232, 255 });
    DrawRectangle((int)x + 62, (int)y + 2, 52, 20, obj.color);
    DrawRectangleLines((int)x + 62, (int)y + 2, 52, 20, BLACK);
}

static void DrawSceneTab(Rectangle panelRect, EditorGuiState &st) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Scene Manager", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    float rowY = panelRect.y + 42.0f;
    const float rowH = 30.0f;
    double now = GetTime();
    for (int i = 0; i < (int)st.objects.size(); i++) {
        EditorObject &obj = st.objects[i];
        Rectangle row = { panelRect.x + 8.0f, rowY, panelRect.width - 16.0f, rowH - 2.0f };
        bool selected = (i == st.selectedIndex);
        Color rowC = selected ? (Color){ 76, 94, 126, 255 } : (Color){ 49, 54, 64, 255 };
        DrawRectangleRounded(row, 0.14f, 5, rowC);
        DrawRectangleRoundedLinesEx(row, 0.14f, 5, 1.0f, (Color){ 90, 98, 114, 255 });

        Rectangle eye = { row.x + 4.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(eye, obj.visible ? "o" : "-", obj.visible ? "Hide object" : "Show object", obj.visible, 15)) {
            obj.visible = !obj.visible;
        }

        Rectangle nameRect = { row.x + 34.0f, row.y + 3.0f, row.width - 94.0f, 22.0f };
        bool nameHover = CheckCollisionPointRec(GetMousePosition(), nameRect);
        bool renamingThis = (st.renameIndex == i && !st.renameFromInspector);
        SetHoverTooltip(nameHover, renamingThis ? "Type name and press Enter" : "Click to select, double click to open inspector");
        if (renamingThis) {
            DrawRectangleRounded(nameRect, 0.10f, 4, (Color){ 33, 39, 47, 255 });
            DrawRectangleRoundedLinesEx(nameRect, 0.10f, 4, 1.0f, (Color){ 124, 157, 230, 255 });
            DrawText(st.renameBuffer, (int)nameRect.x + 4, (int)nameRect.y + 3, 17, RAYWHITE);
        } else {
            DrawText(obj.name.c_str(), (int)nameRect.x + 4, (int)nameRect.y + 3, 17, RAYWHITE);
        }

        Rectangle renameBtn = { row.x + row.width - 54.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(renameBtn, "R", "Rename block", false, 14)) {
            st.selectedIndex = i;
            st.renameIndex = i;
            st.renameFromInspector = false;
            std::snprintf(st.renameBuffer, sizeof(st.renameBuffer), "%s", obj.name.c_str());
        }

        Rectangle deleteBtn = { row.x + row.width - 28.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(deleteBtn, "X", "Delete block", false, 14)) {
            RequestDeleteObject(st, i);
        }

        if (!renamingThis && nameHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            st.selectedIndex = i;
            st.colorSyncIndex = -1;

            if (st.lastSceneClickIndex == i && (now - st.lastSceneClickTime) < 0.26) {
                st.showRightPanel = true;
            }
            st.lastSceneClickIndex = i;
            st.lastSceneClickTime = now;
        }

        if (renamingThis) {
            bool submit = false;
            bool cancel = false;
            UpdateTextFieldBuffer(st.renameBuffer, sizeof(st.renameBuffer), &submit, &cancel);
            if (submit) {
                if (std::strlen(st.renameBuffer) > 0) {
                    char oldName[64] = { 0 };
                    std::snprintf(oldName, sizeof(oldName), "%s", obj.name.c_str());
                    obj.name = st.renameBuffer;
                    char logMsg[196] = { 0 };
                    std::snprintf(logMsg, sizeof(logMsg), "Renamed block '%s' -> '%s'", oldName, obj.name.c_str());
                    AddLog(st, logMsg);
                }
                st.renameIndex = -1;
                st.renameFromInspector = false;
            }
            if (cancel) {
                st.renameIndex = -1;
                st.renameFromInspector = false;
            }
        }

        rowY += rowH;
        if (rowY > panelRect.y + panelRect.height - 12.0f) break;
    }

    if (st.objects.empty()) {
        DrawText("No blocks yet.", (int)panelRect.x + 12, (int)panelRect.y + 52, 18, (Color){ 184, 191, 204, 255 });
    }
}

static void DrawBlocksTab(Rectangle panelRect, EditorGuiState &st, const Rectangle &viewport) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Blocks", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    Rectangle allBtn = { panelRect.x + 10.0f, panelRect.y + 40.0f, 54.0f, 24.0f };
    Rectangle d2Btn = { panelRect.x + 72.0f, panelRect.y + 40.0f, 54.0f, 24.0f };
    Rectangle d3Btn = { panelRect.x + 134.0f, panelRect.y + 40.0f, 54.0f, 24.0f };
    if (DrawButton(allBtn, "All", "Show all primitives", st.filterMode == FilterMode::All, 15)) st.filterMode = FilterMode::All;
    if (DrawButton(d2Btn, "2D", "Show only 2D primitives", st.filterMode == FilterMode::TwoD, 15)) st.filterMode = FilterMode::TwoD;
    if (DrawButton(d3Btn, "3D", "Show only 3D primitives", st.filterMode == FilterMode::ThreeD, 15)) st.filterMode = FilterMode::ThreeD;

    Rectangle listArea = { panelRect.x + 8.0f, panelRect.y + 72.0f, panelRect.width - 16.0f, panelRect.height - 80.0f };
    bool listHover = CheckCollisionPointRec(GetMousePosition(), listArea);
    if (listHover) {
        st.panelScroll -= GetMouseWheelMove() * 26.0f;
        if (st.panelScroll < 0.0f) st.panelScroll = 0.0f;
    }

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    float cellY = listArea.y + 4.0f - st.panelScroll;
    const float cellH = 78.0f;
    const float cellW = listArea.width;

    for (int i = 0; i < kPrimitiveDefCount; i++) {
        const PrimitiveDef &def = kPrimitiveDefs[i];
        if (!PrimitivePassesFilter(def, st.filterMode)) continue;

        Rectangle cell = { listArea.x + 2.0f, cellY, cellW - 4.0f, cellH - 6.0f };
        bool hover = CheckCollisionPointRec(GetMousePosition(), cell);
        SetHoverTooltip(hover, def.tooltip);

        Color fill = hover ? (Color){ 64, 71, 84, 255 } : (Color){ 48, 54, 63, 255 };
        DrawRectangleRounded(cell, 0.12f, 5, fill);
        DrawRectangleRoundedLinesEx(cell, 0.12f, 5, 1.0f, (Color){ 88, 96, 111, 255 });

        DrawPrimitivePreview((Rectangle){ cell.x + 6.0f, cell.y + 5.0f, 64.0f, cell.height - 10.0f }, def.kind, (Color){ 190, 210, 255, 255 });
        DrawText(def.label, (int)cell.x + 78, (int)cell.y + 14, 19, (Color){ 224, 230, 240, 255 });
        DrawText(def.is2D ? "2D" : "3D", (int)cell.x + 78, (int)cell.y + 40, 16, (Color){ 172, 180, 196, 255 });

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            st.palettePressArmed = true;
            st.palettePressedKind = def.kind;
            st.palettePressMouse = GetMousePosition();
            st.draggingPalette = false;
            st.draggingKind = def.kind;
        }

        cellY += cellH;
    }
    EndScissorMode();

    if (st.palettePressArmed && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 d = Vector2Subtract(GetMousePosition(), st.palettePressMouse);
        if (fabsf(d.x) > 6.0f || fabsf(d.y) > 6.0f) {
            st.draggingPalette = true;
        }
    }

    if (st.palettePressArmed && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (st.draggingPalette && CheckCollisionPointRec(GetMousePosition(), viewport)) {
            Vector3 pos = GetMouseWorldOnGround(st);
            SpawnObject(st, st.draggingKind, pos, true);
        } else if (!st.draggingPalette) {
            Vector3 pos = GetSpawnPositionAtTarget(st, st.palettePressedKind);
            SpawnObject(st, st.palettePressedKind, pos, true);
        }
        st.palettePressArmed = false;
        st.draggingPalette = false;
    }

    if (st.draggingPalette || st.palettePressArmed) {
        const PrimitiveDef *def = FindPrimitiveDef(st.draggingKind);
        const char *txt = def ? def->label : "Block";
        Vector2 m = GetMousePosition();
        Rectangle ghost = { m.x + 10.0f, m.y + 10.0f, 90.0f, 30.0f };
        DrawRectangleRounded(ghost, 0.2f, 6, (Color){ 31, 35, 43, 220 });
        DrawRectangleRoundedLinesEx(ghost, 0.2f, 6, 1.0f, (Color){ 96, 109, 128, 255 });
        DrawText(txt, (int)ghost.x + 8, (int)ghost.y + 6, 18, (Color){ 230, 236, 245, 255 });
    }
}

static void DrawColorPanel(Rectangle panelRect, EditorGuiState &st) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Colors", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    if (st.selectedIndex < 0 || st.selectedIndex >= (int)st.objects.size()) {
        DrawText("Select a block first.", (int)panelRect.x + 12, (int)panelRect.y + 52, 18, (Color){ 184, 191, 204, 255 });
        return;
    }

    EditorObject &obj = st.objects[st.selectedIndex];
    if (st.colorSyncIndex != st.selectedIndex) {
        Vector3 hsv = ColorToHSVf(obj.color);
        st.colorHue = hsv.x;
        st.colorSat = hsv.y;
        st.colorVal = hsv.z;
        st.colorSyncIndex = st.selectedIndex;
    }

    Vector2 center = { panelRect.x + panelRect.width * 0.5f, panelRect.y + 122.0f };
    float outerR = 64.0f;
    float innerR = 44.0f;

    for (int deg = 0; deg < 360; deg += 4) {
        DrawRing(center, innerR, outerR, (float)deg, (float)(deg + 4), 2, ColorFromHSV((float)deg, 1.0f, 1.0f));
    }

    Vector2 mouse = GetMousePosition();
    float dist = Vector2Distance(mouse, center);
    if ((IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) &&
        dist >= innerR && dist <= outerR) {
        float angle = atan2f(mouse.y - center.y, mouse.x - center.x) * RAD2DEG;
        if (angle < 0.0f) angle += 360.0f;
        st.colorHue = angle;
    }

    float hueRad = st.colorHue * DEG2RAD;
    float ringR = (innerR + outerR) * 0.5f;
    Vector2 hueMark = { center.x + cosf(hueRad) * ringR, center.y + sinf(hueRad) * ringR };
    DrawCircleV(hueMark, 5.5f, RAYWHITE);
    DrawCircleLines((int)hueMark.x, (int)hueMark.y, 5.5f, BLACK);

    DrawCircleV(center, innerR - 3.0f, ColorFromHSV(st.colorHue, st.colorSat, st.colorVal));
    DrawCircleLines((int)center.x, (int)center.y, innerR - 3.0f, (Color){ 30, 30, 36, 255 });

    Rectangle satRect = { panelRect.x + 22.0f, panelRect.y + 206.0f, panelRect.width - 44.0f, 18.0f };
    for (int i = 0; i < (int)satRect.width; i++) {
        float t = (float)i / satRect.width;
        Color c = ColorFromHSV(st.colorHue, t, st.colorVal);
        DrawLine((int)(satRect.x + i), (int)satRect.y, (int)(satRect.x + i), (int)(satRect.y + satRect.height), c);
    }
    DrawRectangleLinesEx(satRect, 1.0f, (Color){ 95, 103, 118, 255 });

    if ((IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) &&
        CheckCollisionPointRec(mouse, satRect)) {
        st.colorSat = Clampf((mouse.x - satRect.x) / satRect.width, 0.0f, 1.0f);
    }
    float satX = satRect.x + st.colorSat * satRect.width;
    DrawLineEx((Vector2){ satX, satRect.y - 2.0f }, (Vector2){ satX, satRect.y + satRect.height + 2.0f }, 2.0f, BLACK);
    DrawCircleV((Vector2){ satX, satRect.y + satRect.height * 0.5f }, 4.0f, RAYWHITE);

    Rectangle valRect = { panelRect.x + 22.0f, panelRect.y + 236.0f, panelRect.width - 44.0f, 18.0f };
    for (int i = 0; i < (int)valRect.width; i++) {
        float t = (float)i / valRect.width;
        Color c = ColorFromHSV(st.colorHue, st.colorSat, t);
        DrawLine((int)(valRect.x + i), (int)valRect.y, (int)(valRect.x + i), (int)(valRect.y + valRect.height), c);
    }
    DrawRectangleLinesEx(valRect, 1.0f, (Color){ 95, 103, 118, 255 });

    if ((IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) &&
        CheckCollisionPointRec(mouse, valRect)) {
        st.colorVal = Clampf((mouse.x - valRect.x) / valRect.width, 0.0f, 1.0f);
    }
    float valX = valRect.x + st.colorVal * valRect.width;
    DrawLineEx((Vector2){ valX, valRect.y - 2.0f }, (Vector2){ valX, valRect.y + valRect.height + 2.0f }, 2.0f, BLACK);
    DrawCircleV((Vector2){ valX, valRect.y + valRect.height * 0.5f }, 4.0f, RAYWHITE);

    obj.color = ColorFromHSV(st.colorHue, st.colorSat, st.colorVal);
    DrawText("Hue Ring", (int)panelRect.x + 14, (int)panelRect.y + 72, 17, (Color){ 205, 213, 225, 255 });
    DrawText("Saturation", (int)panelRect.x + 22, (int)panelRect.y + 188, 16, (Color){ 190, 198, 210, 255 });
    DrawText("Value", (int)panelRect.x + 22, (int)panelRect.y + 218, 16, (Color){ 190, 198, 210, 255 });

    DrawRectangle((int)panelRect.x + 22, (int)panelRect.y + 270, (int)panelRect.width - 44, 34, obj.color);
    DrawRectangleLines((int)panelRect.x + 22, (int)panelRect.y + 270, (int)panelRect.width - 44, 34, BLACK);
}

static void InitState() {
    if (gState.initialized) return;
    gState.initialized = true;
    gState.showRightPanel = true;
    gState.showSettings = false;
    gState.statusExpanded = false;
    gState.isolateSelected = false;
    gState.viewport2D = false;
    gState.previewOpen = false;
    gState.leftTab = LeftTab::Scene;
    gState.filterMode = FilterMode::All;
    gState.gizmoMode = GizmoMode::Move;
    gState.activeAxis = GizmoAxis::None;
    gState.rightPanelT = 1.0f;
    gState.statusPanelT = 0.0f;
    gState.orbitTarget = { 0.0f, 0.0f, 0.0f };
    gState.orbitYaw = 2.35f;
    gState.orbitPitch = 0.55f;
    gState.orbitDistance = 28.0f;
    gState.cameraOrbitSensitivity = kUserConfig.cameraOrbitSensitivity;
    gState.cameraPanSensitivity = kUserConfig.cameraPanSensitivity;
    gState.cameraZoomSensitivity = kUserConfig.cameraZoomSensitivity;
    gState.gizmoMoveSensitivity = kUserConfig.gizmoMoveSensitivity;
    gState.gizmoRotateSensitivity = kUserConfig.gizmoRotateSensitivity;
    gState.gizmoScaleSensitivity = kUserConfig.gizmoScaleSensitivity;
    gState.previewMoveSpeed = kUserConfig.previewMoveSpeed;
    gState.previewLookSensitivity = kUserConfig.previewLookSensitivity;
    gState.previewZoomSpeed = kUserConfig.previewZoomSpeed;
    gState.previewPanSensitivity = kUserConfig.previewPanSensitivity;
    gState.leftDragNavigate = false;
    gState.draggingObjectMove = false;
    gState.moveDragPlanePoint = { 0.0f, 0.0f, 0.0f };
    gState.moveDragPlaneNormal = { 0.0f, 0.0f, 1.0f };
    gState.moveDragOffset = { 0.0f, 0.0f, 0.0f };
    gState.camera = {};
    gState.camera.fovy = 45.0f;
    gState.camera.projection = CAMERA_PERSPECTIVE;
    gState.previewCamera = {};
    gState.previewCamera.fovy = 60.0f;
    gState.previewCamera.projection = CAMERA_PERSPECTIVE;
    gState.previewYaw = 0.0f;
    gState.previewPitch = 0.0f;
    gState.selectedIndex = -1;
    gState.renameIndex = -1;
    gState.renameFromInspector = false;
    gState.showDeleteConfirm = false;
    gState.deleteTargetIndex = -1;
    gState.deleteTargetName[0] = '\0';
    gState.renameBuffer[0] = '\0';
    gState.lastSceneClickTime = -10.0;
    gState.lastSceneClickIndex = -1;
    gState.palettePressArmed = false;
    gState.draggingPalette = false;
    gState.panelScroll = 0.0f;
    gState.colorHue = 0.0f;
    gState.colorSat = 1.0f;
    gState.colorVal = 1.0f;
    gState.colorSyncIndex = -1;
    UpdateCameraFromOrbit(gState);
    AddLog(gState, "Engine editor ready");
}

void DrawEngineGuiLayout(float dt) {
    InitState();
    gHoverTooltip = nullptr;
    gBlockUiInput = false;

    if (gState.previewOpen) {
        DrawPreviewWindow(gState, dt);
        DrawTooltip();
        return;
    }

    float t = Clampf(dt * kUserConfig.uiAnimationSpeed, 0.0f, 1.0f);
    gState.rightPanelT = LerpF(gState.rightPanelT, gState.showRightPanel ? 1.0f : 0.0f, t);
    gState.statusPanelT = LerpF(gState.statusPanelT, gState.statusExpanded ? 1.0f : 0.0f, t);

    bool textEditing = (gState.renameIndex >= 0);
    bool modalOpen = gState.showDeleteConfirm;
    gBlockUiInput = textEditing || modalOpen;
    if (!textEditing && !modalOpen) {
        if (IsKeyPressed(KEY_W)) gState.gizmoMode = GizmoMode::Move;
        if (IsKeyPressed(KEY_E)) gState.gizmoMode = GizmoMode::Rotate;
        if (IsKeyPressed(KEY_R)) gState.gizmoMode = GizmoMode::Scale;
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) &&
            gState.selectedIndex >= 0 && gState.selectedIndex < (int)gState.objects.size()) {
            RequestDeleteObject(gState, gState.selectedIndex);
        }
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    const float topH = 42.0f;
    const float subTopH = 34.0f;
    const float iconBarW = 68.0f;
    const float leftPanelW = 300.0f;
    const float rightW = 356.0f * gState.rightPanelT;
    const float statusMinH = 28.0f;
    const float statusMaxH = 182.0f;
    const float statusH = statusMinH + (statusMaxH - statusMinH) * gState.statusPanelT;
    const float bodyTop = topH + subTopH;
    const float statusY = (float)sh - statusH;

    Rectangle leftIcons = { 0, bodyTop, iconBarW, statusY - bodyTop };
    Rectangle leftPanel = { iconBarW, bodyTop, leftPanelW, statusY - bodyTop };
    Rectangle viewport = {
        iconBarW + leftPanelW + 1.0f,
        bodyTop + 1.0f,
        (float)sw - iconBarW - leftPanelW - rightW - 2.0f,
        statusY - bodyTop - 2.0f
    };
    if (viewport.width < 180.0f) viewport.width = 180.0f;
    if (viewport.height < 160.0f) viewport.height = 160.0f;

    Rectangle rightPanel = { (float)sw - rightW, bodyTop, rightW, statusY - bodyTop };
    Rectangle statusBar = { 0, statusY, (float)sw, statusH };

    bool mouseInViewport = CheckCollisionPointRec(GetMousePosition(), viewport);
    bool viewportInputEnabled = !gState.viewport2D && mouseInViewport && !gState.draggingPalette && !gState.palettePressArmed && !textEditing && !modalOpen;

    //camera controls no viewport (3d)
    if (viewportInputEnabled) {
        float wheel = GetMouseWheelMove();
        if (fabsf(wheel) > 0.0001f) gState.orbitDistance *= (1.0f - wheel * gState.cameraZoomSensitivity);

        Vector2 d = GetMouseDelta();
        bool altOrbit = (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        bool rightOrbit = IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && !(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        bool panInput = IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
                        (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) ||
                        (gState.leftDragNavigate && IsMouseButtonDown(MOUSE_LEFT_BUTTON));

        if (altOrbit || rightOrbit) {
            gState.orbitYaw -= d.x * gState.cameraOrbitSensitivity;
            gState.orbitPitch += d.y * gState.cameraOrbitSensitivity;
        }

        if (panInput) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(gState.camera.target, gState.camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, gState.camera.up));
            Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
            float panScale = gState.cameraPanSensitivity * gState.orbitDistance;
            gState.orbitTarget = Vector3Add(gState.orbitTarget, Vector3Scale(right, -d.x * panScale));
            gState.orbitTarget = Vector3Add(gState.orbitTarget, Vector3Scale(up, d.y * panScale));
        }
    }
    UpdateCameraFromOrbit(gState);

    //gizmo + selection in viewport
    if (!gState.viewport2D && viewportInputEnabled) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            !(IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
            gState.leftDragNavigate = false;
            gState.activeAxis = GizmoAxis::None;
            gState.draggingObjectMove = false;
            bool handledByGizmo = false;

            if (gState.selectedIndex >= 0 && gState.selectedIndex < (int)gState.objects.size()) {
                EditorObject &selectedObj = gState.objects[gState.selectedIndex];
                if (!selectedObj.anchored) {
                    GizmoAxis hoverAxis = DetectGizmoAxis(gState, viewport, selectedObj);
                    if (hoverAxis != GizmoAxis::None) {
                        gState.activeAxis = hoverAxis;
                        handledByGizmo = true;
                    }
                }
            }

            if (!handledByGizmo) {
                int picked = PickObject3D(gState, viewport);
                gState.selectedIndex = picked;
                gState.colorSyncIndex = -1;

                if (picked >= 0 && picked < (int)gState.objects.size()) {
                    EditorObject &obj = gState.objects[picked];
                    if (!obj.anchored) {
                        if (gState.gizmoMode == GizmoMode::Move) {
                            Ray ray = GetMouseRay(GetMousePosition(), gState.camera);
                            Vector3 forward = Vector3Normalize(Vector3Subtract(gState.camera.target, gState.camera.position));
                            if (Vector3LengthSqr(forward) < 0.0001f) forward = (Vector3){ 0.0f, 0.0f, 1.0f };
                            gState.moveDragPlaneNormal = forward;
                            gState.moveDragPlanePoint = obj.position;

                            Vector3 hit = obj.position;
                            if (RayPlaneIntersection(ray, gState.moveDragPlanePoint, gState.moveDragPlaneNormal, &hit)) {
                                gState.moveDragOffset = Vector3Subtract(obj.position, hit);
                            } else {
                                gState.moveDragOffset = (Vector3){ 0.0f, 0.0f, 0.0f };
                            }
                            gState.draggingObjectMove = true;
                        } else {
                            GizmoAxis hoverAxis = DetectGizmoAxis(gState, viewport, obj);
                            if (hoverAxis != GizmoAxis::None) gState.activeAxis = hoverAxis;
                        }
                    }
                } else {
                    gState.leftDragNavigate = true;
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && gState.selectedIndex >= 0 && gState.selectedIndex < (int)gState.objects.size()) {
            EditorObject &obj = gState.objects[gState.selectedIndex];
            if (!obj.anchored && gState.gizmoMode == GizmoMode::Move) {
                if (gState.activeAxis != GizmoAxis::None) {
                    ApplyMoveAxisDrag(gState, obj, gState.activeAxis, GetMouseDelta());
                } else if (gState.draggingObjectMove) {
                    Ray ray = GetMouseRay(GetMousePosition(), gState.camera);
                    Vector3 hit = obj.position;
                    if (RayPlaneIntersection(ray, gState.moveDragPlanePoint, gState.moveDragPlaneNormal, &hit)) {
                        obj.position = Vector3Add(hit, gState.moveDragOffset);
                    }
                }
            } else if (!obj.anchored && gState.activeAxis != GizmoAxis::None &&
                       (gState.gizmoMode == GizmoMode::Rotate || gState.gizmoMode == GizmoMode::Scale)) {
                ApplyGizmoDrag(obj, gState.gizmoMode, gState.activeAxis, GetMouseDelta(),
                               gState.gizmoRotateSensitivity, gState.gizmoScaleSensitivity);
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            gState.leftDragNavigate = false;
            gState.draggingObjectMove = false;
            gState.activeAxis = GizmoAxis::None;
        }
    } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        gState.leftDragNavigate = false;
        gState.draggingObjectMove = false;
        gState.activeAxis = GizmoAxis::None;
    }

    DrawRectangle(0, 0, sw, sh, (Color){ 19, 22, 28, 255 });

    //top bar
    DrawRectangle(0, 0, sw, (int)topH, (Color){ 36, 40, 48, 255 });
    DrawLineEx((Vector2){ 0, topH }, (Vector2){ (float)sw, topH }, 1.0f, (Color){ 82, 88, 100, 255 });
    DrawButton((Rectangle){ 14, 7, 82, 28 }, "New", "Create new scene");
    DrawButton((Rectangle){ 104, 7, 88, 28 }, "Open", "Open scene");
    DrawButton((Rectangle){ 200, 7, 86, 28 }, "Save", "Save scene");
    DrawButton((Rectangle){ 294, 7, 98, 28 }, "Export", "Export build");
    if (DrawButton((Rectangle){ 400, 7, 112, 28 }, "Settings", "Open editor settings", gState.showSettings)) {
        gState.showSettings = !gState.showSettings;
    }
    DrawText("Le Bombo Game Engine", 530, 10, 20, (Color){ 230, 233, 240, 255 });

    //sub bar
    DrawRectangle(0, (int)topH, sw, (int)subTopH, (Color){ 31, 35, 42, 255 });
    DrawLineEx((Vector2){ 0, bodyTop }, (Vector2){ (float)sw, bodyTop }, 1.0f, (Color){ 78, 84, 96, 255 });

    Rectangle btn2D = { (float)sw - 470.0f, topH + 3.0f, 50.0f, 27.0f };
    Rectangle btn3D = { (float)sw - 414.0f, topH + 3.0f, 50.0f, 27.0f };
    Rectangle btnMove = { (float)sw - 355.0f, topH + 3.0f, 58.0f, 27.0f };
    Rectangle btnRotate = { (float)sw - 292.0f, topH + 3.0f, 58.0f, 27.0f };
    Rectangle btnScale = { (float)sw - 229.0f, topH + 3.0f, 58.0f, 27.0f };
    Rectangle btnPreview = { (float)sw - 165.0f, topH + 3.0f, 98.0f, 27.0f };
    Rectangle btnEye = { (float)sw - 61.0f, topH + 3.0f, 22.0f, 27.0f };
    Rectangle btnRight = { (float)sw - 33.0f, topH + 3.0f, 26.0f, 27.0f };

    if (DrawButton(btn2D, "2D", "Switch viewport to 2D mode", gState.viewport2D)) gState.viewport2D = true;
    if (DrawButton(btn3D, "3D", "Switch viewport to 3D mode", !gState.viewport2D)) gState.viewport2D = false;
    if (DrawButton(btnMove, "Move", "Move gizmo mode (W)", gState.gizmoMode == GizmoMode::Move)) gState.gizmoMode = GizmoMode::Move;
    if (DrawButton(btnRotate, "Rotate", "Rotate gizmo mode (E)", gState.gizmoMode == GizmoMode::Rotate)) gState.gizmoMode = GizmoMode::Rotate;
    if (DrawButton(btnScale, "Scale", "Scale gizmo mode (R)", gState.gizmoMode == GizmoMode::Scale)) gState.gizmoMode = GizmoMode::Scale;
    if (DrawButton(btnPreview, "> Preview", "Open live preview window")) {
        OpenPreview(gState);
    }
    if (DrawButton(btnEye, "o", "Show only selected block", gState.isolateSelected, 16)) gState.isolateSelected = !gState.isolateSelected;
    if (DrawButton(btnRight, gState.showRightPanel ? "<" : ">", "Toggle inspector panel", false, 16)) gState.showRightPanel = !gState.showRightPanel;

    //left icon bar
    DrawRectangleRec(leftIcons, (Color){ 33, 37, 44, 255 });
    DrawRectangleLinesEx(leftIcons, 1.0f, (Color){ 78, 85, 97, 255 });
    if (DrawButton((Rectangle){ 14, bodyTop + 16, 40, 40 }, "SCN", "Scene manager", gState.leftTab == LeftTab::Scene, 14)) gState.leftTab = LeftTab::Scene;
    if (DrawButton((Rectangle){ 14, bodyTop + 66, 40, 40 }, "BLK", "Blocks palette", gState.leftTab == LeftTab::Blocks, 14)) gState.leftTab = LeftTab::Blocks;
    if (DrawButton((Rectangle){ 14, bodyTop + 116, 40, 40 }, "CLR", "Color tools", gState.leftTab == LeftTab::Colors, 14)) gState.leftTab = LeftTab::Colors;

    //left content panel
    if (gState.leftTab == LeftTab::Scene) {
        DrawSceneTab(leftPanel, gState);
    } else if (gState.leftTab == LeftTab::Blocks) {
        DrawBlocksTab(leftPanel, gState, viewport);
    } else {
        DrawColorPanel(leftPanel, gState);
    }

    //viewport
    if (gState.viewport2D) {
        Draw2DGrid(viewport);
        Draw2DViewportObjects(viewport, gState);
    } else {
        Draw3DViewport(viewport, gState);
    }

    //right panel
    if (rightW > 1.0f) {
        DrawInspectorPanel(rightPanel, gState);
    }

    //status bar
    DrawRectangleRec(statusBar, (Color){ 29, 33, 39, 255 });
    DrawRectangleLinesEx(statusBar, 1.0f, (Color){ 78, 84, 96, 255 });
    DrawRectangle(0, (int)statusY, sw, 28, (Color){ 38, 42, 50, 255 });
    if (DrawButton((Rectangle){ 8, statusY + 2.0f, 30, 24 }, gState.statusExpanded ? "v" : "^", "Expand/collapse status panel", false, 16)) {
        gState.statusExpanded = !gState.statusExpanded;
    }
    DrawText("Status: Console", 46, (int)statusY + 6, 18, (Color){ 208, 214, 226, 255 });
    if (statusH > 40.0f) {
        DrawText("Event Log (date/time):", 18, (int)statusY + 36, 17, (Color){ 195, 203, 217, 255 });
        int maxLines = (int)((statusH - 54.0f) / 18.0f);
        if (maxLines < 1) maxLines = 1;

        int total = (int)gState.logs.size();
        int start = total - maxLines;
        if (start < 0) start = 0;

        int lineY = (int)statusY + 56;
        if (total <= 0) {
            DrawText("[empty] no actions yet", 18, lineY, 16, (Color){ 168, 177, 192, 255 });
        } else {
            for (int i = start; i < total; i++) {
                const EngineLogEntry &entry = gState.logs[i];
                char line[256] = { 0 };
                std::snprintf(line, sizeof(line), "[%s] %s", entry.timestamp, entry.text.c_str());
                DrawText(line, 18, lineY, 16, (Color){ 176, 184, 198, 255 });
                lineY += 18;
            }
        }
    } else if (!gState.logs.empty()) {
        const EngineLogEntry &entry = gState.logs.back();
        char line[256] = { 0 };
        std::snprintf(line, sizeof(line), "[%s] %s", entry.timestamp, entry.text.c_str());
        DrawText(line, 212, (int)statusY + 7, 15, (Color){ 176, 184, 198, 255 });
    }

    if (gState.showSettings) {
        DrawSettingsPopup((Rectangle){ 14.0f, bodyTop + 10.0f, 340.0f, 352.0f }, gState);
    }

    DrawDeleteConfirmPopup(gState);

    DrawTooltip();
}
