#include "../include/gui_workflow.hpp"
#include "../include/gui_manager.hpp"
#include "../include/piece_library.hpp"
#include "../include/workspace_tabs.hpp"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// implements workflow logic object operations viewport rendering panels and preview
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

// shortens one absolute path into relative like main/GameEngine/... and trims to fit width
static std::string BuildCompactPathLabel(const std::string &path, float maxWidth, int fontSize) {
    if (path.empty()) return "pieces path: (not set)";

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    const std::string marker = "/main/GameEngine/";
    size_t markerPos = normalized.find(marker);
    if (markerPos != std::string::npos && markerPos + 1 < normalized.size()) {
        normalized = normalized.substr(markerPos + 1);
    }

    if (MeasureText(normalized.c_str(), fontSize) <= (int)maxWidth) return normalized;

    const std::string ellipsis = "...";
    size_t keepFront = std::min<size_t>(24, normalized.size());
    size_t keepBack = std::min<size_t>(24, normalized.size() - std::min<size_t>(24, normalized.size()));
    if (keepBack < 6) keepBack = std::min<size_t>(12, normalized.size());

    std::string compact = normalized.substr(0, keepFront) + ellipsis +
                          normalized.substr(normalized.size() - keepBack);
    while (MeasureText(compact.c_str(), fontSize) > (int)maxWidth && (keepFront > 6 || keepBack > 6)) {
        if (keepFront >= keepBack && keepFront > 6) keepFront--;
        else if (keepBack > 6) keepBack--;
        compact = normalized.substr(0, keepFront) + ellipsis +
                  normalized.substr(normalized.size() - keepBack);
    }
    return compact;
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

// checks if one object layer is currently visible
static bool IsLayerVisible(const EditorGuiState &st, int layerIndex) {
    if (layerIndex < 0 || layerIndex >= (int)st.layers.size()) return true;
    return st.layers[layerIndex].visible;
}

// computes hierarchy depth for one object by parent chain
static int ComputeHierarchyDepth(const std::vector<EditorObject> &objects, int index) {
    int depth = 0;
    int parent = (index >= 0 && index < (int)objects.size()) ? objects[index].parentIndex : -1;
    while (parent >= 0 && parent < (int)objects.size() && depth < 6) {
        depth++;
        parent = objects[parent].parentIndex;
    }
    return depth;
}

bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3 *hitOut) {
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
    obj.layerIndex = st.activeLayerIndex >= 0 && st.activeLayerIndex < (int)st.layers.size() ? st.activeLayerIndex : 0;
    obj.parentIndex = -1;
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
        st.selectedIndices.clear();
        st.selectedIndices.push_back(st.selectedIndex);
        st.colorSyncIndex = -1;
    }
}

void RequestDeleteObject(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.objects.size()) return;
    st.showDeleteConfirm = true;
    st.deleteTargetIndex = index;
    std::snprintf(st.deleteTargetName, sizeof(st.deleteTargetName), "%s", st.objects[index].name.c_str());
}

void RequestDeleteSelection(EditorGuiState &st) {
    if (st.selectedIndices.empty()) return;
    st.showDeleteConfirm = true;
    st.deleteTargetIndex = -2;
    std::snprintf(st.deleteTargetName, sizeof(st.deleteTargetName), "%d selected blocks", (int)st.selectedIndices.size());
}

static void DeleteObjectAtIndex(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.objects.size()) return;

    std::string deletedName = st.objects[index].name;
    st.objects.erase(st.objects.begin() + index);

    for (EditorObject &obj : st.objects) {
        if (obj.parentIndex == index) obj.parentIndex = -1;
        else if (obj.parentIndex > index) obj.parentIndex--;
    }

    for (Shotpoint &sp : st.shotpoints) {
        if (sp.objectIndex == index) sp.objectIndex = -1;
        else if (sp.objectIndex > index) sp.objectIndex--;
    }

    for (int &idx : st.selectedIndices) {
        if (idx == index) idx = -1;
        else if (idx > index) idx--;
    }
    std::vector<int> filtered;
    filtered.reserve(st.selectedIndices.size());
    for (int idx : st.selectedIndices) {
        if (idx >= 0 && idx < (int)st.objects.size()) filtered.push_back(idx);
    }
    st.selectedIndices = filtered;

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

// deletes all currently selected objects in descending index order
static void DeleteSelectedObjects(EditorGuiState &st) {
    if (st.selectedIndices.empty()) return;

    std::vector<int> sorted = st.selectedIndices;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    int removed = 0;
    for (int i = (int)sorted.size() - 1; i >= 0; i--) {
        int idx = sorted[i];
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        DeleteObjectAtIndex(st, idx);
        removed++;
    }

    st.selectedIndices.clear();
    st.selectedIndex = -1;
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;

    char msg[96] = { 0 };
    std::snprintf(msg, sizeof(msg), "Deleted %d selected blocks", removed);
    AddLog(st, msg);
}

static float ApproxObjectRadius(const EditorObject &obj) {
    float scaleLen = Vector3Length(obj.scale);
    if (scaleLen < 0.2f) scaleLen = 0.2f;
    return scaleLen * (obj.is2D ? 0.55f : 0.85f);
}

int PickObject3D(const EditorGuiState &st, const Rectangle &viewport) {
    if (!CheckCollisionPointRec(GetMousePosition(), viewport)) return -1;

    Ray ray = GetMouseRay(GetMousePosition(), st.camera);
    float bestDist = 1e30f;
    int bestIdx = -1;
    for (int i = 0; i < (int)st.objects.size(); i++) {
        const EditorObject &obj = st.objects[i];
        if (!obj.visible) continue;
        if (!IsLayerVisible(st, obj.layerIndex)) continue;
        if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;

        RayCollision hit = GetRayCollisionSphere(ray, obj.position, ApproxObjectRadius(obj));
        if (hit.hit && hit.distance < bestDist) {
            bestDist = hit.distance;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void UpdateCameraFromOrbit(EditorGuiState &st) {
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

// build transform matrix using translate rotate scale order
static Matrix BuildObjectMatrix(const EditorObject &obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixTranslate(obj.position.x, obj.position.y, obj.position.z));
    mat = MatrixMultiply(mat, MatrixRotateX(obj.rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj.rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj.rotation.z * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixScale(obj.scale.x, obj.scale.y, obj.scale.z));
    return mat;
}

// build rotation only matrix for direction vectors
static Matrix BuildObjectRotationMatrix(const EditorObject &obj) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixRotateX(obj.rotation.x * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateY(obj.rotation.y * DEG2RAD));
    mat = MatrixMultiply(mat, MatrixRotateZ(obj.rotation.z * DEG2RAD));
    return mat;
}

// resolve one shotpoint into world position and direction
bool BuildShotpointWorld(const EditorGuiState &st, const Shotpoint &sp, Vector3 *outPos, Vector3 *outDir) {
    if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) return false;

    const EditorObject &obj = st.objects[sp.objectIndex];
    Matrix objMat = BuildObjectMatrix(obj);
    Matrix rotMat = BuildObjectRotationMatrix(obj);

    if (outPos) *outPos = Vector3Transform(sp.localPos, objMat);
    if (outDir) {
        Vector3 localDir = Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, rotMat);
        if (Vector3LengthSqr(localDir) < 0.0001f) localDir = (Vector3){ 0.0f, 0.0f, -1.0f };
        *outDir = Vector3Normalize(localDir);
    }

    return true;
}

void OpenPreview(EditorGuiState &st) {
    Vector3 look = Vector3Normalize(Vector3Subtract(st.camera.target, st.camera.position));
    if (Vector3LengthSqr(look) < 0.00001f) look = (Vector3){ 0.0f, 0.0f, -1.0f };

    st.previewOpen = true;
    st.previewMode = PreviewMode::Scene;
    st.previewShotpointIndex = -1;
    st.previewBlastTimer = 0.0f;
    st.previewCamera = {};
    st.previewCamera.position = st.camera.position;
    st.previewCamera.fovy = 60.0f;
    st.previewCamera.projection = CAMERA_PERSPECTIVE;
    st.previewYaw = atan2f(look.x, look.z);
    st.previewPitch = asinf(Clampf(look.y, -1.0f, 1.0f));
    UpdatePreviewCameraDirection(st);
    AddLog(st, "Preview opened");
}

// open preview focused on all blasts or one selected shotpoint
void OpenBlastPreview(EditorGuiState &st, int shotpointIndex) {
    OpenPreview(st);
    if (shotpointIndex >= 0) {
        st.previewMode = PreviewMode::SingleBlast;
        st.previewShotpointIndex = shotpointIndex;
    } else {
        st.previewMode = PreviewMode::AllBlasts;
        st.previewShotpointIndex = -1;
    }
}

static Vector3 AxisVectorFromGizmoAxis(GizmoAxis axis) {
    if (axis == GizmoAxis::X) return (Vector3){ 1.0f, 0.0f, 0.0f };
    if (axis == GizmoAxis::Y) return (Vector3){ 0.0f, 1.0f, 0.0f };
    if (axis == GizmoAxis::Z) return (Vector3){ 0.0f, 0.0f, 1.0f };
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

void DrawEditorObject3D(const EditorObject &obj) {
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

GizmoAxis DetectGizmoAxis(const EditorGuiState &st, const Rectangle &viewport, const EditorObject &obj) {
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

void ApplyGizmoDrag(EditorObject &obj, GizmoMode mode, GizmoAxis axis, Vector2 delta, float rotateSensitivity, float scaleSensitivity) {
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

void ApplyMoveAxisDrag(EditorGuiState &st, EditorObject &obj, GizmoAxis axis, Vector2 mouseDelta) {
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

void DrawTooltip() {
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

void Draw2DViewportObjects(const Rectangle &viewport, const EditorGuiState &st) {
    float scale = 24.0f;
    Vector2 center = { viewport.x + viewport.width * 0.5f, viewport.y + viewport.height * 0.5f };

    for (int i = 0; i < (int)st.objects.size(); i++) {
        const EditorObject &obj = st.objects[i];
        if (!obj.visible) continue;
        if (!IsLayerVisible(st, obj.layerIndex)) continue;
        if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;

        Vector2 p = { center.x + obj.position.x * scale, center.y + obj.position.z * scale };
        float sz = 10.0f + Vector3Length(obj.scale) * 3.0f;
        DrawCircleV(p, sz, obj.color);
        DrawCircleLines((int)p.x, (int)p.y, sz, BLACK);

        bool selected = (i == st.selectedIndex);
        if (!selected) {
            for (int idx : st.selectedIndices) {
                if (idx == i) {
                    selected = true;
                    break;
                }
            }
        }
        if (selected) {
            DrawCircleLines((int)p.x, (int)p.y, sz + 5.0f, (Color){ 255, 236, 130, 255 });
        }
    }
}

void Draw2DGrid(const Rectangle &viewport) {
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

void Draw3DViewport(const Rectangle &viewport, EditorGuiState &st) {
    BeginScissorMode((int)viewport.x + 1, (int)viewport.y + 1, (int)viewport.width - 2, (int)viewport.height - 2);
    DrawRectangleRec(viewport, (Color){ 23, 26, 32, 255 });

    BeginMode3D(st.camera);
        DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 240, 240 }, (Color){ 24, 31, 35, 255 });
        DrawGrid(160, 1.0f);

        for (int i = 0; i < (int)st.objects.size(); i++) {
            const EditorObject &obj = st.objects[i];
            if (!obj.visible) continue;
            if (!IsLayerVisible(st, obj.layerIndex)) continue;
            if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;
            DrawEditorObject3D(obj);
        }

        float blink = 0.35f + 0.65f * fabsf(sinf((float)GetTime() * 5.2f));
        for (int i = 0; i < (int)st.shotpoints.size(); i++) {
            const Shotpoint &sp = st.shotpoints[i];
            if (!sp.enabled) continue;
            if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) continue;
            const EditorObject &owner = st.objects[sp.objectIndex];
            if (!owner.visible || !IsLayerVisible(st, owner.layerIndex)) continue;
            Vector3 shotPos = { 0 };
            Vector3 shotDir = { 0, 0, -1 };
            if (!BuildShotpointWorld(st, sp, &shotPos, &shotDir)) continue;
            float r = Clampf(sp.blastSize * 0.85f, 0.05f, 0.22f);
            DrawSphere(shotPos, r, Fade(sp.blastColor, blink));
            if (i == st.selectedShotpoint && st.leftTab == LeftTab::Lasers) {
                DrawSphereWires(shotPos, r * 1.8f, 10, 10, Fade(WHITE, blink));
            }
        }

        if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
            const EditorObject &obj = st.objects[st.selectedIndex];
            if (obj.visible && IsLayerVisible(st, obj.layerIndex)) {
                DrawSelectionHighlight(obj);
                if (!obj.anchored) {
                    DrawGizmoVisual(obj, st.gizmoMode, st.activeAxis);
                }
            }
        }

        for (int idx : st.selectedIndices) {
            if (idx == st.selectedIndex) continue;
            if (idx < 0 || idx >= (int)st.objects.size()) continue;
            const EditorObject &obj = st.objects[idx];
            if (!obj.visible || !IsLayerVisible(st, obj.layerIndex)) continue;
            DrawSelectionHighlight(obj);
        }
    EndMode3D();

    EndScissorMode();

    DrawRectangleLinesEx(viewport, 2.0f, (Color){ 90, 98, 114, 255 });
    DrawText("3D Viewport", (int)viewport.x + 14, (int)viewport.y + 10, 20, (Color){ 214, 220, 232, 255 });

    if (st.selectMode && st.boxSelecting) {
        const float minX = fminf(st.boxSelectStart.x, st.boxSelectEnd.x);
        const float minY = fminf(st.boxSelectStart.y, st.boxSelectEnd.y);
        const float maxX = fmaxf(st.boxSelectStart.x, st.boxSelectEnd.x);
        const float maxY = fmaxf(st.boxSelectStart.y, st.boxSelectEnd.y);
        Rectangle box = { minX, minY, maxX - minX, maxY - minY };
        if (box.x < viewport.x) {
            box.width -= (viewport.x - box.x);
            box.x = viewport.x;
        }
        if (box.y < viewport.y) {
            box.height -= (viewport.y - box.y);
            box.y = viewport.y;
        }
        float right = box.x + box.width;
        float bottom = box.y + box.height;
        float vpRight = viewport.x + viewport.width;
        float vpBottom = viewport.y + viewport.height;
        if (right > vpRight) box.width = vpRight - box.x;
        if (bottom > vpBottom) box.height = vpBottom - box.y;
        if (box.width < 0.0f) box.width = 0.0f;
        if (box.height < 0.0f) box.height = 0.0f;
        DrawRectangleRec(box, (Color){ 93, 147, 255, 52 });
        DrawRectangleLinesEx(box, 1.6f, (Color){ 148, 184, 255, 255 });
    }
}

// draw animated blasts from shotpoints in preview mode
static void DrawPreviewBlasts(EditorGuiState &st) {
    st.previewBlastTimer += GetFrameTime();
    const float cadence = 1.25f;

    for (int i = 0; i < (int)st.shotpoints.size(); i++) {
        const Shotpoint &sp = st.shotpoints[i];
        if (!sp.enabled) continue;
        if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) continue;
        if (!st.objects[sp.objectIndex].visible || !IsLayerVisible(st, st.objects[sp.objectIndex].layerIndex)) continue;
        if (st.previewMode == PreviewMode::SingleBlast && i != st.previewShotpointIndex) continue;

        Vector3 worldPos = { 0.0f, 0.0f, 0.0f };
        Vector3 worldDir = { 0.0f, 0.0f, -1.0f };
        if (!BuildShotpointWorld(st, sp, &worldPos, &worldDir)) continue;

        float phase = fmodf(st.previewBlastTimer + (float)i * 0.22f, cadence);
        float alphaPulse = 0.4f + 0.6f * fabsf(sinf((float)GetTime() * 4.0f + (float)i));
        float markerR = 0.08f + 0.03f * alphaPulse;
        DrawSphere(worldPos, markerR, Fade(sp.blastColor, alphaPulse));

        if (phase <= 0.72f) {
            float blastLen = 4.0f + (phase / 0.72f) * 34.0f;
            float radius = Clampf(sp.blastSize, 0.05f, 1.8f);
            Vector3 end = Vector3Add(worldPos, Vector3Scale(worldDir, blastLen));
            DrawCylinderEx(worldPos, end, radius, radius * 0.88f, 8, Fade(sp.blastColor, 0.88f));
            DrawSphere(worldPos, radius * 1.5f, Fade(WHITE, 0.25f));
        }
    }
}

void DrawPreviewWindow(EditorGuiState &st, float dt) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // close preview quickly with keyboard
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

    // mouse look + pan controls in preview window
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

    // keyboard rotate controls for preview
    float keyLook = st.previewLookSensitivity * 200.0f * dt;
    if (IsKeyDown(KEY_LEFT)) st.previewYaw -= keyLook;
    if (IsKeyDown(KEY_RIGHT)) st.previewYaw += keyLook;
    if (IsKeyDown(KEY_UP)) st.previewPitch += keyLook;
    if (IsKeyDown(KEY_DOWN)) st.previewPitch -= keyLook;

    // keyboard move controls for preview
    float speedMul = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 2.5f : 1.0f;
    float move = st.previewMoveSpeed * speedMul * dt;
    if (IsKeyDown(KEY_W)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(flatForward, move));
    if (IsKeyDown(KEY_S)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(flatForward, -move));
    if (IsKeyDown(KEY_A)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(right, -move));
    if (IsKeyDown(KEY_D)) st.previewCamera.position = Vector3Add(st.previewCamera.position, Vector3Scale(right, move));

    // mouse wheel zoom in/out following view direction
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
            if (!IsLayerVisible(st, obj.layerIndex)) continue;
            if (st.isolateSelected && st.selectedIndex >= 0 && i != st.selectedIndex) continue;
            DrawEditorObject3D(obj);
        }

        if (st.previewMode == PreviewMode::AllBlasts || st.previewMode == PreviewMode::SingleBlast) {
            DrawPreviewBlasts(st);
        }
    EndMode3D();

    DrawRectangle(0, 0, sw, 40, (Color){ 15, 18, 24, 230 });
    DrawLine(0, 40, sw, 40, (Color){ 80, 88, 103, 255 });

    const char *title = "Preview Window";
    if (st.previewMode == PreviewMode::AllBlasts) title = "Preview Window - Blasts";
    if (st.previewMode == PreviewMode::SingleBlast) title = "Preview Window - Shotpoint";
    DrawText(title, 12, 10, 21, RAYWHITE);
    DrawText("WASD move | Arrow keys rotate | RMB look | MMB pan | Mouse wheel zoom | Shift speed", 260, 12, 16, (Color){ 190, 199, 214, 255 });
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

void DrawSettingsPopup(Rectangle rect, EditorGuiState &st) {
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

void DrawDeleteConfirmPopup(EditorGuiState &st) {
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
    if (st.deleteTargetIndex == -2) std::snprintf(msg, sizeof(msg), "Delete %s?", st.deleteTargetName);
    else std::snprintf(msg, sizeof(msg), "Delete block '%s'?", st.deleteTargetName);
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
        if (st.deleteTargetIndex == -2) DeleteSelectedObjects(st);
        else DeleteObjectAtIndex(st, st.deleteTargetIndex);
        st.showDeleteConfirm = false;
    }
    gBlockUiInput = prevBlock;
}

void DrawInspectorPanel(Rectangle panelRect, EditorGuiState &st) {
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

    y += 34.0f;
    DrawText("Layer", (int)x, (int)y + 4, 18, (Color){ 198, 206, 219, 255 });
    Rectangle layerPrev = { x + 60.0f, y, 26.0f, 24.0f };
    Rectangle layerNext = { x + w - 26.0f, y, 26.0f, 24.0f };
    Rectangle layerValue = { x + 92.0f, y, w - 124.0f, 24.0f };
    if (DrawButton(layerPrev, "<", "select previous layer", false, 15) && !st.layers.empty()) {
        obj.layerIndex--;
        if (obj.layerIndex < 0) obj.layerIndex = (int)st.layers.size() - 1;
    }
    if (DrawButton(layerNext, ">", "select next layer", false, 15) && !st.layers.empty()) {
        obj.layerIndex++;
        if (obj.layerIndex >= (int)st.layers.size()) obj.layerIndex = 0;
    }
    if (obj.layerIndex < 0 || obj.layerIndex >= (int)st.layers.size()) obj.layerIndex = 0;
    DrawRectangleRounded(layerValue, 0.14f, 6, (Color){ 35, 40, 49, 255 });
    DrawRectangleRoundedLinesEx(layerValue, 0.14f, 6, 1.0f, (Color){ 81, 89, 104, 255 });
    const char *layerName = st.layers.empty() ? "Default" : st.layers[obj.layerIndex].name.c_str();
    DrawText(layerName, (int)layerValue.x + 6, (int)layerValue.y + 4, 16, RAYWHITE);

    y += 30.0f;
    DrawText("Parent", (int)x, (int)y + 4, 18, (Color){ 198, 206, 219, 255 });
    Rectangle parentPrev = { x + 60.0f, y, 26.0f, 24.0f };
    Rectangle parentNext = { x + w - 26.0f, y, 26.0f, 24.0f };
    Rectangle parentValue = { x + 92.0f, y, w - 124.0f, 24.0f };
    if (DrawButton(parentPrev, "<", "select previous parent", false, 15)) {
        int next = obj.parentIndex - 1;
        if (next == st.selectedIndex) next--;
        if (next < -1) next = (int)st.objects.size() - 1;
        if (next == st.selectedIndex) next = -1;
        obj.parentIndex = next;
    }
    if (DrawButton(parentNext, ">", "select next parent", false, 15)) {
        int next = obj.parentIndex + 1;
        if (next == st.selectedIndex) next++;
        if (next >= (int)st.objects.size()) next = -1;
        if (next == st.selectedIndex) next = -1;
        obj.parentIndex = next;
    }
    DrawRectangleRounded(parentValue, 0.14f, 6, (Color){ 35, 40, 49, 255 });
    DrawRectangleRoundedLinesEx(parentValue, 0.14f, 6, 1.0f, (Color){ 81, 89, 104, 255 });
    const char *parentName = "none";
    if (obj.parentIndex >= 0 && obj.parentIndex < (int)st.objects.size()) parentName = st.objects[obj.parentIndex].name.c_str();
    DrawText(parentName, (int)parentValue.x + 6, (int)parentValue.y + 4, 16, RAYWHITE);

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

void DrawSceneTab(Rectangle panelRect, EditorGuiState &st) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Scene Manager", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    Rectangle layerToggle = { panelRect.x + 8.0f, panelRect.y + 40.0f, 96.0f, 24.0f };
    Rectangle layerAdd = { panelRect.x + 110.0f, panelRect.y + 40.0f, 52.0f, 24.0f };
    Rectangle layerAssign = { panelRect.x + 8.0f, panelRect.y + 68.0f, panelRect.width - 16.0f, 24.0f };
    if (DrawButton(layerToggle, st.showLayerPanel ? "Layers on" : "Layers off", "toggle layer panel", st.showLayerPanel, 14)) {
        st.showLayerPanel = !st.showLayerPanel;
    }
    if (DrawButton(layerAdd, "+L", "add new layer", false, 15)) {
        EditorLayer layer = {};
        char name[48] = { 0 };
        std::snprintf(name, sizeof(name), "Layer_%02d", (int)st.layers.size() + 1);
        layer.name = name;
        layer.visible = true;
        st.layers.push_back(layer);
        st.activeLayerIndex = (int)st.layers.size() - 1;
    }
    if (DrawButton(layerAssign, "set selected to active layer", "move selected block into active layer", false, 13)) {
        if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size() &&
            st.activeLayerIndex >= 0 && st.activeLayerIndex < (int)st.layers.size()) {
            st.objects[st.selectedIndex].layerIndex = st.activeLayerIndex;
        }
    }

    float rowY = panelRect.y + 98.0f;
    if (st.showLayerPanel) {
        Rectangle layerList = { panelRect.x + 8.0f, rowY, panelRect.width - 16.0f, 92.0f };
        DrawRectangleRounded(layerList, 0.07f, 6, (Color){ 29, 33, 40, 255 });
        DrawRectangleRoundedLinesEx(layerList, 0.07f, 6, 1.0f, (Color){ 84, 92, 108, 255 });

        if (CheckCollisionPointRec(GetMousePosition(), layerList)) {
            st.layerScroll -= GetMouseWheelMove() * 20.0f;
            if (st.layerScroll < 0.0f) st.layerScroll = 0.0f;
        }

        BeginScissorMode((int)layerList.x + 1, (int)layerList.y + 1, (int)layerList.width - 2, (int)layerList.height - 2);
        float ly = layerList.y + 4.0f - st.layerScroll;
        for (int i = 0; i < (int)st.layers.size(); i++) {
            EditorLayer &layer = st.layers[i];
            Rectangle item = { layerList.x + 4.0f, ly, layerList.width - 8.0f, 22.0f };
            DrawRectangleRounded(item, 0.10f, 4, i == st.activeLayerIndex ? (Color){ 78, 98, 132, 255 } : (Color){ 45, 50, 60, 255 });
            DrawRectangleRoundedLinesEx(item, 0.10f, 4, 1.0f, (Color){ 83, 92, 108, 255 });

            Rectangle eye = { item.x + 3.0f, item.y + 1.0f, 20.0f, 20.0f };
            if (DrawButton(eye, layer.visible ? "o" : "-", layer.visible ? "hide layer" : "show layer", layer.visible, 14)) {
                layer.visible = !layer.visible;
            }
            DrawText(layer.name.c_str(), (int)item.x + 30, (int)item.y + 3, 16, RAYWHITE);
            if (CheckCollisionPointRec(GetMousePosition(), item) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) st.activeLayerIndex = i;
            ly += 24.0f;
        }
        EndScissorMode();
        rowY += layerList.height + 8.0f;
    }

    Rectangle objectList = { panelRect.x + 8.0f, rowY, panelRect.width - 16.0f, panelRect.height - (rowY - panelRect.y) - 8.0f };
    DrawRectangleRounded(objectList, 0.07f, 6, (Color){ 29, 33, 40, 255 });
    DrawRectangleRoundedLinesEx(objectList, 0.07f, 6, 1.0f, (Color){ 84, 92, 108, 255 });

    if (CheckCollisionPointRec(GetMousePosition(), objectList)) {
        st.sceneScroll -= GetMouseWheelMove() * 24.0f;
        if (st.sceneScroll < 0.0f) st.sceneScroll = 0.0f;
    }

    const float rowH = 30.0f;
    double now = GetTime();
    BeginScissorMode((int)objectList.x + 1, (int)objectList.y + 1, (int)objectList.width - 2, (int)objectList.height - 2);
    float drawY = objectList.y + 4.0f - st.sceneScroll;
    for (int i = 0; i < (int)st.objects.size(); i++) {
        EditorObject &obj = st.objects[i];
        const int depth = ComputeHierarchyDepth(st.objects, i);
        Rectangle row = { objectList.x + 4.0f, drawY, objectList.width - 8.0f, rowH - 2.0f };
        bool selected = (i == st.selectedIndex);
        Color rowC = selected ? (Color){ 76, 94, 126, 255 } : (Color){ 49, 54, 64, 255 };
        DrawRectangleRounded(row, 0.14f, 5, rowC);
        DrawRectangleRoundedLinesEx(row, 0.14f, 5, 1.0f, (Color){ 90, 98, 114, 255 });

        Rectangle eye = { row.x + 4.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(eye, obj.visible ? "o" : "-", obj.visible ? "hide object" : "show object", obj.visible, 15)) {
            obj.visible = !obj.visible;
        }

        float indent = 34.0f + (float)depth * 12.0f;
        for (int d = 0; d < depth; d++) {
            float lx = row.x + 30.0f + (float)d * 12.0f;
            DrawLineEx((Vector2){ lx, row.y + 3.0f }, (Vector2){ lx, row.y + row.height - 3.0f }, 1.0f, (Color){ 92, 101, 116, 255 });
        }
        if (depth > 0) {
            float hx0 = row.x + 30.0f + (float)(depth - 1) * 12.0f;
            float hx1 = row.x + indent - 6.0f;
            DrawLineEx((Vector2){ hx0, row.y + row.height * 0.5f }, (Vector2){ hx1, row.y + row.height * 0.5f }, 1.2f, (Color){ 108, 122, 146, 255 });
        }
        Rectangle nameRect = { row.x + indent, row.y + 3.0f, row.width - indent - 64.0f, 22.0f };
        bool nameHover = CheckCollisionPointRec(GetMousePosition(), nameRect);
        bool renamingThis = (st.renameIndex == i && !st.renameFromInspector);
        SetHoverTooltip(nameHover, renamingThis ? "type name and press enter" : "click to select double click to inspect");
        if (renamingThis) {
            DrawRectangleRounded(nameRect, 0.10f, 4, (Color){ 33, 39, 47, 255 });
            DrawRectangleRoundedLinesEx(nameRect, 0.10f, 4, 1.0f, (Color){ 124, 157, 230, 255 });
            DrawText(st.renameBuffer, (int)nameRect.x + 4, (int)nameRect.y + 3, 17, RAYWHITE);
        } else {
            DrawText(obj.name.c_str(), (int)nameRect.x + 4, (int)nameRect.y + 3, 17, RAYWHITE);
        }

        Rectangle renameBtn = { row.x + row.width - 54.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(renameBtn, "R", "rename block", false, 14)) {
            st.selectedIndex = i;
            st.renameIndex = i;
            st.renameFromInspector = false;
            std::snprintf(st.renameBuffer, sizeof(st.renameBuffer), "%s", obj.name.c_str());
        }

        Rectangle deleteBtn = { row.x + row.width - 28.0f, row.y + 3.0f, 24.0f, 22.0f };
        if (DrawButton(deleteBtn, "X", "delete block", false, 14)) {
            RequestDeleteObject(st, i);
        }

        if (!renamingThis && nameHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            st.selectedIndex = i;
            st.selectedIndices.clear();
            st.selectedIndices.push_back(i);
            st.colorSyncIndex = -1;
            if (st.lastSceneClickIndex == i && (now - st.lastSceneClickTime) < 0.26) st.showRightPanel = true;
            st.lastSceneClickIndex = i;
            st.lastSceneClickTime = now;
        }

        if (renamingThis) {
            bool submit = false;
            bool cancel = false;
            UpdateTextFieldBuffer(st.renameBuffer, sizeof(st.renameBuffer), &submit, &cancel);
            if (submit) {
                if (std::strlen(st.renameBuffer) > 0) obj.name = st.renameBuffer;
                st.renameIndex = -1;
                st.renameFromInspector = false;
            }
            if (cancel) {
                st.renameIndex = -1;
                st.renameFromInspector = false;
            }
        }

        drawY += rowH;
    }
    EndScissorMode();

    if (st.objects.empty()) {
        DrawText("No blocks yet.", (int)objectList.x + 10, (int)objectList.y + 10, 18, (Color){ 184, 191, 204, 255 });
    }
}

void DrawBlocksTab(Rectangle panelRect, EditorGuiState &st, const Rectangle &viewport) {
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

    Rectangle primArea = { panelRect.x + 8.0f, panelRect.y + 72.0f, panelRect.width - 16.0f, panelRect.height * 0.56f };
    Rectangle pieceHead = { panelRect.x + 8.0f, primArea.y + primArea.height + 4.0f, panelRect.width - 16.0f, 28.0f };
    Rectangle pieceArea = { panelRect.x + 8.0f, pieceHead.y + pieceHead.height + 2.0f, panelRect.width - 16.0f, panelRect.height - (pieceHead.y + pieceHead.height + 10.0f - panelRect.y) };
    if (pieceArea.height < 70.0f) pieceArea.height = 70.0f;

    bool primHover = CheckCollisionPointRec(GetMousePosition(), primArea);
    bool pieceHover = CheckCollisionPointRec(GetMousePosition(), pieceArea);
    if (primHover) {
        st.panelScroll -= GetMouseWheelMove() * 26.0f;
        if (st.panelScroll < 0.0f) st.panelScroll = 0.0f;
    }
    if (pieceHover) {
        st.piecesScroll -= GetMouseWheelMove() * 20.0f;
        if (st.piecesScroll < 0.0f) st.piecesScroll = 0.0f;
    }

    DrawRectangleRounded(primArea, 0.06f, 6, (Color){ 29, 33, 40, 255 });
    DrawRectangleRoundedLinesEx(primArea, 0.06f, 6, 1.0f, (Color){ 78, 86, 101, 255 });

    BeginScissorMode((int)primArea.x + 1, (int)primArea.y + 1, (int)primArea.width - 2, (int)primArea.height - 2);
    float cellY = primArea.y + 4.0f - st.panelScroll;
    const float cellH = 78.0f;
    const float cellW = primArea.width;

    for (int i = 0; i < kPrimitiveDefCount; i++) {
        const PrimitiveDef &def = kPrimitiveDefs[i];
        if (!PrimitivePassesFilter(def, st.filterMode)) continue;

        Rectangle cell = { primArea.x + 2.0f, cellY, cellW - 4.0f, cellH - 6.0f };
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

    DrawRectangleRounded(pieceHead, 0.07f, 6, (Color){ 39, 45, 54, 255 });
    DrawRectangleRoundedLinesEx(pieceHead, 0.07f, 6, 1.0f, (Color){ 85, 95, 112, 255 });
    DrawText("Pieces", (int)pieceHead.x + 8, (int)pieceHead.y + 6, 17, (Color){ 224, 230, 240, 255 });
    if (DrawButton((Rectangle){ pieceHead.x + pieceHead.width - 88.0f, pieceHead.y + 2.0f, 34.0f, 24.0f }, "+", "create piece from selection", false, 18)) {
        CreatePieceFromSelection(st);
        ReloadPieceLibrary(st);
    }
    if (DrawButton((Rectangle){ pieceHead.x + pieceHead.width - 50.0f, pieceHead.y + 2.0f, 42.0f, 24.0f }, "R", "reload pieces folder", false, 14)) {
        ReloadPieceLibrary(st);
    }

    DrawRectangleRounded(pieceArea, 0.06f, 6, (Color){ 29, 33, 40, 255 });
    DrawRectangleRoundedLinesEx(pieceArea, 0.06f, 6, 1.0f, (Color){ 78, 86, 101, 255 });
    BeginScissorMode((int)pieceArea.x + 1, (int)pieceArea.y + 1, (int)pieceArea.width - 2, (int)pieceArea.height - 2);
    float py = pieceArea.y + 4.0f - st.piecesScroll;
    for (int i = 0; i < (int)st.pieceLibrary.size(); i++) {
        const PieceTemplate &piece = st.pieceLibrary[i];
        Rectangle row = { pieceArea.x + 4.0f, py, pieceArea.width - 8.0f, 24.0f };
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRounded(row, 0.10f, 4, hover ? (Color){ 64, 71, 84, 255 } : (Color){ 45, 50, 60, 255 });
        DrawRectangleRoundedLinesEx(row, 0.10f, 4, 1.0f, (Color){ 83, 92, 108, 255 });
        DrawText(piece.name.c_str(), (int)row.x + 6, (int)row.y + 4, 16, RAYWHITE);

        Rectangle openBtn = { row.x + row.width - 72.0f, row.y + 1.0f, 32.0f, 22.0f };
        Rectangle addBtn = { row.x + row.width - 36.0f, row.y + 1.0f, 32.0f, 22.0f };
        bool onOpen = CheckCollisionPointRec(GetMousePosition(), openBtn);
        bool onAdd = CheckCollisionPointRec(GetMousePosition(), addBtn);
        if (DrawButton(openBtn, "Ed", "open piece workspace tab", false, 13)) {
            int tab = FindWorkspaceByPath(st, piece.filePath);
            if (tab < 0) tab = AddWorkspace(st, WorkspaceKind::Piece, piece.name, piece.filePath, piece.snapshot);
            st.requestSwitchWorkspace = true;
            st.requestSwitchWorkspaceIndex = tab;
        }
        if (DrawButton(addBtn, "+", "insert piece in viewport center", false, 15)) {
            Vector3 pos = GetSpawnPositionAtTarget(st, PrimitiveKind::Cube);
            SpawnPieceTemplateAt(st, i, pos, true);
        } else if (hover && !onOpen && !onAdd && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector3 pos = GetSpawnPositionAtTarget(st, PrimitiveKind::Cube);
            SpawnPieceTemplateAt(st, i, pos, true);
        }
        py += 28.0f;
    }
    EndScissorMode();
    if (st.pieceLibrary.empty()) {
        DrawText("no piece files found", (int)pieceArea.x + 8, (int)pieceArea.y + 8, 16, (Color){ 184, 191, 204, 255 });
    }

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

void DrawMakePieceTab(Rectangle panelRect, EditorGuiState &st, const Rectangle &viewport) {
    DrawRectangleRec(panelRect, (Color){ 34, 39, 47, 255 });
    DrawRectangleLinesEx(panelRect, 1.0f, (Color){ 78, 86, 100, 255 });
    DrawRectangle((int)panelRect.x, (int)panelRect.y, (int)panelRect.width, 34, (Color){ 43, 48, 57, 255 });
    DrawText("Make Your Own Piece", (int)panelRect.x + 10, (int)panelRect.y + 8, 20, (Color){ 225, 231, 240, 255 });

    Rectangle createBtn = { panelRect.x + 10.0f, panelRect.y + 40.0f, 138.0f, 26.0f };
    Rectangle reloadBtn = { panelRect.x + 156.0f, panelRect.y + 40.0f, 64.0f, 26.0f };
    Rectangle saveBtn = { panelRect.x + 226.0f, panelRect.y + 40.0f, 64.0f, 26.0f };
    if (DrawButton(createBtn, "new from selected", "create piece file from current selection", false, 14)) {
        CreatePieceFromSelection(st);
        ReloadPieceLibrary(st);
    }
    if (DrawButton(reloadBtn, "reload", "reload .piece files from folder", false, 14)) ReloadPieceLibrary(st);
    if (DrawButton(saveBtn, "save", "save active piece tab", false, 14)) SaveActivePieceWorkspace(st);

    const std::string pathLabel = BuildCompactPathLabel(st.piecesRootPath, panelRect.width - 20.0f, 14);
    DrawText(pathLabel.c_str(), (int)panelRect.x + 10, (int)panelRect.y + 72, 14, (Color){ 170, 180, 196, 255 });

    float dirY = panelRect.y + 92.0f;
    DrawText("Vehicle Forward", (int)panelRect.x + 10, (int)dirY, 17, (Color){ 214, 222, 236, 255 });
    if (DrawFloatStepper("Yaw", &st.vehicleForwardYawDeg, 5.0f, -180.0f, 180.0f, panelRect.x + 10.0f, dirY + 22.0f, panelRect.width - 20.0f, "forward direction offset in degrees")) {
        st.vehicleForwardYawDeg = Clampf(st.vehicleForwardYawDeg, -180.0f, 180.0f);
    }
    Rectangle frontBtn = { panelRect.x + 10.0f, dirY + 52.0f, 64.0f, 22.0f };
    Rectangle rightBtn = { panelRect.x + 78.0f, dirY + 52.0f, 64.0f, 22.0f };
    Rectangle backBtn = { panelRect.x + 146.0f, dirY + 52.0f, 64.0f, 22.0f };
    Rectangle leftBtn = { panelRect.x + 214.0f, dirY + 52.0f, 64.0f, 22.0f };
    if (DrawButton(frontBtn, "Front", "set forward to -Z", fabsf(st.vehicleForwardYawDeg - 0.0f) < 0.1f, 14)) st.vehicleForwardYawDeg = 0.0f;
    if (DrawButton(rightBtn, "Right", "set forward to +X", fabsf(st.vehicleForwardYawDeg - 90.0f) < 0.1f, 14)) st.vehicleForwardYawDeg = 90.0f;
    if (DrawButton(backBtn, "Back", "set forward to +Z", fabsf(st.vehicleForwardYawDeg - 180.0f) < 0.1f, 14)) st.vehicleForwardYawDeg = 180.0f;
    if (DrawButton(leftBtn, "Left", "set forward to -X", fabsf(st.vehicleForwardYawDeg + 90.0f) < 0.1f, 14)) st.vehicleForwardYawDeg = -90.0f;

    Rectangle listArea = { panelRect.x + 8.0f, panelRect.y + 172.0f, panelRect.width - 16.0f, panelRect.height - 180.0f };
    if (listArea.height < 60.0f) listArea.height = 60.0f;
    DrawRectangleRounded(listArea, 0.06f, 6, (Color){ 29, 33, 40, 255 });
    DrawRectangleRoundedLinesEx(listArea, 0.06f, 6, 1.0f, (Color){ 78, 86, 101, 255 });

    if (CheckCollisionPointRec(GetMousePosition(), listArea)) {
        st.piecesScroll -= GetMouseWheelMove() * 20.0f;
        if (st.piecesScroll < 0.0f) st.piecesScroll = 0.0f;
    }

    BeginScissorMode((int)listArea.x + 1, (int)listArea.y + 1, (int)listArea.width - 2, (int)listArea.height - 2);
    float y = listArea.y + 4.0f - st.piecesScroll;
    for (int i = 0; i < (int)st.pieceLibrary.size(); i++) {
        const PieceTemplate &piece = st.pieceLibrary[i];
        Rectangle row = { listArea.x + 4.0f, y, listArea.width - 8.0f, 26.0f };
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRounded(row, 0.10f, 4, hover ? (Color){ 64, 71, 84, 255 } : (Color){ 45, 50, 60, 255 });
        DrawRectangleRoundedLinesEx(row, 0.10f, 4, 1.0f, (Color){ 83, 92, 108, 255 });
        DrawText(piece.name.c_str(), (int)row.x + 8, (int)row.y + 5, 16, RAYWHITE);

        Rectangle openBtn = { row.x + row.width - 100.0f, row.y + 2.0f, 44.0f, 22.0f };
        Rectangle spawnBtn = { row.x + row.width - 52.0f, row.y + 2.0f, 48.0f, 22.0f };
        if (DrawButton(openBtn, "open", "open piece workspace tab", false, 13)) {
            int tab = FindWorkspaceByPath(st, piece.filePath);
            if (tab < 0) tab = AddWorkspace(st, WorkspaceKind::Piece, piece.name, piece.filePath, piece.snapshot);
            st.requestSwitchWorkspace = true;
            st.requestSwitchWorkspaceIndex = tab;
        }
        if (DrawButton(spawnBtn, "spawn", "insert piece in current viewport target", false, 13)) {
            Vector3 pos = CheckCollisionPointRec(GetMousePosition(), viewport) ? GetMouseWorldOnGround(st) : GetSpawnPositionAtTarget(st, PrimitiveKind::Cube);
            SpawnPieceTemplateAt(st, i, pos, true);
        }
        y += 30.0f;
    }
    EndScissorMode();

    if (st.pieceLibrary.empty()) {
        DrawText("no piece templates available", (int)listArea.x + 10, (int)listArea.y + 10, 16, (Color){ 184, 191, 204, 255 });
    }
}

void DrawColorPanel(Rectangle panelRect, EditorGuiState &st) {
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
