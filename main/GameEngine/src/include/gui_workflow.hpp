#ifndef GAMEENGINE_GUI_WORKFLOW_HPP
#define GAMEENGINE_GUI_WORKFLOW_HPP

#include "config.hpp"

#include "raylib.h"

void OpenPreview(EditorGuiState &st);
void OpenBlastPreview(EditorGuiState &st, int shotpointIndex);
void DrawPreviewWindow(EditorGuiState &st, float dt);
void DrawTooltip();
void RequestDeleteObject(EditorGuiState &st, int index);
void RequestDeleteSelection(EditorGuiState &st);
void DrawEditorObject3D(const EditorObject &obj);
bool BuildShotpointWorld(const EditorGuiState &st, const Shotpoint &sp, Vector3 *outPos, Vector3 *outDir);
void UpdateCameraFromOrbit(EditorGuiState &st);
GizmoAxis DetectGizmoAxis(const EditorGuiState &st, const Rectangle &viewport, const EditorObject &obj);
int PickObject3D(const EditorGuiState &st, const Rectangle &viewport);
bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3 *hitOut);
void ApplyMoveAxisDrag(EditorGuiState &st, EditorObject &obj, GizmoAxis axis, Vector2 mouseDelta);
void ApplyGizmoDrag(EditorObject &obj, GizmoMode mode, GizmoAxis axis, Vector2 delta, float rotateSensitivity, float scaleSensitivity);
void Draw2DViewportObjects(const Rectangle &viewport, const EditorGuiState &st);
void Draw2DGrid(const Rectangle &viewport);
void Draw3DViewport(const Rectangle &viewport, EditorGuiState &st);
void DrawSettingsPopup(Rectangle rect, EditorGuiState &st);
void DrawDeleteConfirmPopup(EditorGuiState &st);
void DrawInspectorPanel(Rectangle panelRect, EditorGuiState &st);
void DrawSceneTab(Rectangle panelRect, EditorGuiState &st);
void DrawBlocksTab(Rectangle panelRect, EditorGuiState &st, const Rectangle &viewport);
void DrawMakePieceTab(Rectangle panelRect, EditorGuiState &st, const Rectangle &viewport);
void DrawColorPanel(Rectangle panelRect, EditorGuiState &st);

#endif
