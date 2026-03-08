#ifndef GAMEENGINE_CONFIG_HPP
#define GAMEENGINE_CONFIG_HPP

#include "raylib.h"

#include <string>
#include <vector>

enum class LeftTab {
    Scene = 0,
    Blocks,
    Colors,
    MakePiece,
    Lasers
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

enum class PreviewMode {
    Scene = 0,
    AllBlasts,
    SingleBlast
};

enum class ProjectDialogMode {
    None = 0,
    NewProject,
    OpenProject
};

enum class WorkspaceKind {
    Project = 0,
    Piece
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
    int maxUndoEntries;
    int maxCacheEntries;
};

extern const EngineUserConfig kUserConfig;

struct EditorObject {
    std::string name;
    PrimitiveKind kind;
    bool is2D;
    bool visible;
    bool anchored;
    int layerIndex;
    int parentIndex;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    Color color;
};

struct Shotpoint {
    std::string name;
    int objectIndex;
    bool enabled;
    Vector3 localPos;
    Color blastColor;
    float blastSize;
};

struct EditorSnapshot {
    std::vector<EditorObject> objects;
    std::vector<Shotpoint> shotpoints;
    std::vector<std::string> layerNames;
    std::vector<bool> layerVisible;
};

struct EditorLayer {
    std::string name;
    bool visible;
};

struct PieceTemplate {
    std::string name;
    std::string filePath;
    EditorSnapshot snapshot;
};

struct WorkspaceTab {
    WorkspaceKind kind;
    std::string name;
    std::string filePath;
    EditorSnapshot snapshot;
    bool dirty;
};

struct EngineLogEntry {
    char timestamp[32];
    std::string text;
};

struct CacheActionEntry {
    char timestamp[32];
    std::string action;
    std::string detail;
};

struct EditorGuiState {
    bool initialized;
    bool showLeftPanel;
    bool showRightPanel;
    bool showSettings;
    bool statusExpanded;
    bool isolateSelected;
    bool selectMode;
    bool boxSelecting;
    bool viewport2D;
    bool previewOpen;
    bool shotpointPlacementOpen;
    LeftTab leftTab;
    FilterMode filterMode;
    GizmoMode gizmoMode;
    GizmoAxis activeAxis;
    PreviewMode previewMode;
    float leftPanelT;
    float rightPanelT;
    float statusPanelT;
    Vector2 boxSelectStart;
    Vector2 boxSelectEnd;
    std::vector<int> selectedIndices;
    bool selectionDragActive;
    Vector2 selectionDragStart;

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
    int previewShotpointIndex;
    float previewBlastTimer;

    Camera3D shotpointPlacementCamera;
    float shotpointPlacementYaw;
    float shotpointPlacementPitch;
    int shotpointPlacementIndex;

    float panelScroll;
    int selectedIndex;
    int activeLayerIndex;
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
    bool showLayerPanel;
    bool showNewMenu;
    bool showTabContextMenu;
    int tabContextIndex;
    Vector2 tabContextPos;
    int tabRenameIndex;
    char tabRenameBuffer[96];

    float colorHue;
    float colorSat;
    float colorVal;
    int colorSyncIndex;

    float vehicleForwardYawDeg;

    GizmoAxis shotpointPlacementAxis;
    bool shotpointPlacementDrag;

    float piecesScroll;
    float sceneScroll;
    float layerScroll;

    bool historyPending;
    bool historyApplying;
    double historyLastChangeTime;
    std::string historyBaselineHash;
    EditorSnapshot historyBaselineSnapshot;
    EditorSnapshot historyPendingBeforeSnapshot;
    std::vector<EditorSnapshot> undoStack;
    std::vector<EditorSnapshot> redoStack;

    std::string cacheFilePath;
    bool cacheInitialized;
    std::vector<CacheActionEntry> cacheActions;

    bool showProjectDialog;
    ProjectDialogMode projectDialogMode;
    bool showUnsavedConfirm;
    ProjectDialogMode pendingDialogMode;
    bool showRecoveryDialog;
    std::string recoveryProjectPath;
    char projectNameBuffer[128];
    char projectErrorBuffer[192];
    float projectListScroll;
    std::vector<std::string> projectFolderList;

    std::string projectsRootPath;
    std::string currentProjectPath;
    std::string currentProjectName;
    std::string piecesRootPath;
    bool projectLoaded;
    bool projectDirty;
    double autosaveIntervalSec;
    double autosaveLastTime;

    std::vector<EngineLogEntry> logs;
    std::vector<EditorObject> objects;
    std::vector<Shotpoint> shotpoints;
    std::vector<EditorLayer> layers;
    std::vector<PieceTemplate> pieceLibrary;
    std::vector<EditorObject> clipboardObjects;
    std::vector<Shotpoint> clipboardShotpoints;
    int clipboardPasteCount;
    std::vector<WorkspaceTab> workspaces;
    int activeWorkspaceIndex;
    std::vector<Rectangle> workspaceTabRects;
    int hoveredWorkspaceTab;
    bool requestSwitchWorkspace;
    int requestSwitchWorkspaceIndex;
    int selectedShotpoint;
};

#endif
