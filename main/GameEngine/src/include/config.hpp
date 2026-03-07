#ifndef GAMEENGINE_CONFIG_HPP
#define GAMEENGINE_CONFIG_HPP

#include "raylib.h"

#include <string>
#include <vector>

enum class LeftTab {
    Scene = 0,
    Blocks,
    Colors
};

enum class FilterMode {
    All = 0,
    TwoD,
    ThreeD
};

enum class GizmoMode {
    Move = 0,
    Rotate,
    Scale
};

enum class GizmoAxis {
    None = 0,
    X,
    Y,
    Z
};

enum class PrimitiveKind {
    Cube = 0,
    Sphere,
    Cylinder,
    Cone,
    Plane,
    Rectangle2D,
    Circle2D,
    Triangle2D,
    Ring2D,
    Poly2D,
    Line2D
};

struct PrimitiveDef {
    PrimitiveKind kind;
    const char *label;
    bool is2D;
    const char *tooltip;
};

extern const PrimitiveDef kPrimitiveDefs[];
extern const int kPrimitiveDefCount;

struct EngineUserConfig {
    float uiAnimationSpeed;

    float cameraOrbitSensitivity;
    float cameraPanSensitivity;
    float cameraZoomSensitivity;

    float gizmoMoveSensitivity;
    float gizmoRotateSensitivity;
    float gizmoScaleSensitivity;

    float previewMoveSpeed;
    float previewLookSensitivity;
    float previewZoomSpeed;
    float previewPanSensitivity;

    int maxLogEntries;
    int trimLogEntries;
};

extern const EngineUserConfig kUserConfig;

struct EditorObject {
    std::string name;
    PrimitiveKind kind;
    bool is2D;
    bool visible;
    bool anchored;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    Color color;
};

struct EngineLogEntry {
    char timestamp[32];
    std::string text;
};

struct EditorGuiState {
    bool initialized;
    bool showRightPanel;
    bool showSettings;
    bool statusExpanded;
    bool isolateSelected;
    bool viewport2D;
    bool previewOpen;
    LeftTab leftTab;
    FilterMode filterMode;
    GizmoMode gizmoMode;
    GizmoAxis activeAxis;
    float rightPanelT;
    float statusPanelT;

    Camera3D camera;
    Vector3 orbitTarget;
    float orbitYaw;
    float orbitPitch;
    float orbitDistance;
    float cameraOrbitSensitivity;
    float cameraPanSensitivity;
    float cameraZoomSensitivity;
    float gizmoMoveSensitivity;
    float gizmoRotateSensitivity;
    float gizmoScaleSensitivity;
    float previewMoveSpeed;
    float previewLookSensitivity;
    float previewZoomSpeed;
    float previewPanSensitivity;
    bool leftDragNavigate;
    bool draggingObjectMove;
    Vector3 moveDragPlanePoint;
    Vector3 moveDragPlaneNormal;
    Vector3 moveDragOffset;
    Camera3D previewCamera;
    float previewYaw;
    float previewPitch;

    float panelScroll;
    int selectedIndex;
    int renameIndex;
    bool renameFromInspector;
    bool showDeleteConfirm;
    int deleteTargetIndex;
    char deleteTargetName[64];
    char renameBuffer[64];
    double lastSceneClickTime;
    int lastSceneClickIndex;

    bool palettePressArmed;
    PrimitiveKind palettePressedKind;
    Vector2 palettePressMouse;
    bool draggingPalette;
    PrimitiveKind draggingKind;

    float colorHue;
    float colorSat;
    float colorVal;
    int colorSyncIndex;

    std::vector<EngineLogEntry> logs;
    std::vector<EditorObject> objects;
};

#endif
