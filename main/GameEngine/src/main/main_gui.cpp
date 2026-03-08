#include "../include/main_gui.hpp"
#include "../include/gui_manager.hpp"
#include "../include/gui_shotpoints.hpp"
#include "../include/gui_workflow.hpp"
#include "../include/piece_library.hpp"
#include "../include/persistence.hpp"
#include "../include/vehicle_import.hpp"
#include "../include/vehicle_export.hpp"
#include "../include/workspace_tabs.hpp"

#include <cstdio>
#include <string>

// builds a default tab label when explicit name is empty
static std::string BuildFallbackTabName(const WorkspaceTab &tab, int projectOrdinal, int pieceOrdinal) {
    if (!tab.name.empty()) return tab.name;
    if (tab.kind == WorkspaceKind::Project) return "pj" + std::to_string(projectOrdinal);
    return "piece " + std::to_string(pieceOrdinal);
}

// generates a new unique tab label for one workspace kind
static std::string NextWorkspaceName(WorkspaceKind kind) {
    int count = 0;
    for (const WorkspaceTab &tab : gState.workspaces) {
        if (tab.kind == kind) count++;
    }
    if (kind == WorkspaceKind::Project) return "pj" + std::to_string(count + 1);
    return "piece " + std::to_string(count + 1);
}

// returns one readable mirror axis label for toolbar
static const char *MirrorAxisLabel(MirrorAxis axis) {
    if (axis == MirrorAxis::X) return "X";
    if (axis == MirrorAxis::Y) return "Y";
    return "Z";
}

// calculates an adaptive button width that fits current text
static float FitButtonWidth(const char *label, int fontSize, float minWidth, float pad) {
    if (label == nullptr || label[0] == '\0') return minWidth;
    float w = (float)MeasureText(label, fontSize) + pad;
    if (w < minWidth) w = minWidth;
    return w;
}

// opens inline rename mode for one workspace tab
static void StartWorkspaceRename(int index) {
    if (index < 0 || index >= (int)gState.workspaces.size()) return;
    gState.tabRenameIndex = index;
    std::snprintf(gState.tabRenameBuffer, sizeof(gState.tabRenameBuffer), "%s", gState.workspaces[index].name.c_str());
}

// draws one right click context menu for workspace tabs
static void DrawWorkspaceTabContextMenu(void) {
    if (!gState.showTabContextMenu) return;
    if (gState.tabContextIndex < 0 || gState.tabContextIndex >= (int)gState.workspaces.size()) {
        gState.showTabContextMenu = false;
        gState.tabContextIndex = -1;
        return;
    }

    Rectangle menu = { gState.tabContextPos.x, gState.tabContextPos.y, 154.0f, 96.0f };
    if (menu.x + menu.width > (float)GetScreenWidth() - 6.0f) menu.x = (float)GetScreenWidth() - menu.width - 6.0f;
    if (menu.y + menu.height > (float)GetScreenHeight() - 6.0f) menu.y = (float)GetScreenHeight() - menu.height - 6.0f;

    DrawRectangleRounded(menu, 0.12f, 6, (Color){ 30, 35, 44, 248 });
    DrawRectangleRoundedLinesEx(menu, 0.12f, 6, 1.2f, (Color){ 98, 110, 130, 255 });

    Rectangle renameBtn = { menu.x + 6.0f, menu.y + 6.0f, menu.width - 12.0f, 24.0f };
    Rectangle dupBtn = { menu.x + 6.0f, menu.y + 36.0f, menu.width - 12.0f, 24.0f };
    Rectangle closeBtn = { menu.x + 6.0f, menu.y + 66.0f, menu.width - 12.0f, 24.0f };

    if (DrawButton(renameBtn, "Rename", "rename selected tab", false, 15)) {
        StartWorkspaceRename(gState.tabContextIndex);
        gState.showTabContextMenu = false;
    }
    if (DrawButton(dupBtn, "Duplicate", "duplicate selected tab", false, 15)) {
        int idx = DuplicateWorkspace(gState, gState.tabContextIndex, NextWorkspaceName(gState.workspaces[gState.tabContextIndex].kind));
        if (idx >= 0) {
            gState.requestSwitchWorkspace = true;
            gState.requestSwitchWorkspaceIndex = idx;
        }
        gState.showTabContextMenu = false;
    }
    if (DrawButton(closeBtn, "Close", "close selected tab", false, 15)) {
        CloseWorkspace(gState, gState.tabContextIndex);
        gState.showTabContextMenu = false;
    }

    Vector2 mouse = GetMousePosition();
    bool inside = CheckCollisionPointRec(mouse, menu);
    if (!inside && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gState.showTabContextMenu = false;
    }
}

// draws workspace tabs and updates hover and switch requests
static void DrawWorkspaceTabsStrip(float x, float y, float h, float maxWidth) {
    gState.workspaceTabRects.clear();
    gState.hoveredWorkspaceTab = -1;

    float tabX = x;
    const float tabGap = 5.0f;
    int projectCount = 0;
    int pieceCount = 0;

    int requestClose = -1;
    int requestSwitch = -1;

    for (int i = 0; i < (int)gState.workspaces.size(); i++) {
        WorkspaceTab &tab = gState.workspaces[i];
        if (tab.kind == WorkspaceKind::Project) projectCount++;
        else pieceCount++;

        std::string label = BuildFallbackTabName(tab, projectCount, pieceCount);
        if (tab.dirty) label += "*";

        int tw = MeasureText(label.c_str(), 14);
        float tabW = (float)(tw + 44);
        if (tabW < 94.0f) tabW = 94.0f;

        Rectangle rect = { tabX, y, tabW, h };
        if (rect.x + rect.width > x + maxWidth) break;

        bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
        if (hover) gState.hoveredWorkspaceTab = i;
        gState.workspaceTabRects.push_back(rect);

        Rectangle closeRect = { rect.x + rect.width - 22.0f, rect.y + 3.0f, 18.0f, rect.height - 6.0f };
        Rectangle labelRect = { rect.x + 8.0f, rect.y + 2.0f, rect.width - 32.0f, rect.height - 4.0f };

        bool active = i == gState.activeWorkspaceIndex;
        DrawButton(rect, "", "switch workspace tab", active, 14);

        if (gState.tabRenameIndex == i) {
            DrawRectangleRounded(labelRect, 0.12f, 5, (Color){ 30, 35, 43, 255 });
            DrawRectangleRoundedLinesEx(labelRect, 0.12f, 5, 1.0f, (Color){ 126, 161, 236, 255 });
            DrawText(gState.tabRenameBuffer, (int)labelRect.x + 5, (int)labelRect.y + 4, 15, RAYWHITE);

            bool submit = false;
            bool cancel = false;
            UpdateTextFieldBuffer(gState.tabRenameBuffer, sizeof(gState.tabRenameBuffer), &submit, &cancel);
            if (submit) {
                if (gState.tabRenameBuffer[0] != '\0') RenameWorkspace(gState, i, gState.tabRenameBuffer);
                gState.tabRenameIndex = -1;
            }
            if (cancel) gState.tabRenameIndex = -1;
        } else {
            DrawText(label.c_str(), (int)labelRect.x + 1, (int)labelRect.y + 4, 15, RAYWHITE);
            if (hover && !CheckCollisionPointRec(GetMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) requestSwitch = i;
        }

        bool canClose = gState.workspaces.size() > 1;
        if (canClose && DrawButton(closeRect, "x", "close tab", false, 13)) requestClose = i;

        if (hover && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            gState.showTabContextMenu = true;
            gState.tabContextIndex = i;
            gState.tabContextPos = GetMousePosition();
        }

        if (gState.selectionDragActive && gState.hoveredWorkspaceTab == i && i != gState.activeWorkspaceIndex) {
            DrawRectangleRoundedLinesEx(rect, 0.18f, 6, 2.0f, (Color){ 240, 204, 120, 255 });
        }

        tabX += tabW + tabGap;
    }

    Rectangle newPieceBtn = { tabX, y, 84.0f, h };
    if (newPieceBtn.x + newPieceBtn.width <= x + maxWidth) {
        if (DrawButton(newPieceBtn, "+ Piece", "create piece from current selection", false, 15)) {
            CreatePieceFromSelection(gState);
            ReloadPieceLibrary(gState);
        }
    }

    if (requestClose >= 0) {
        if (gState.tabRenameIndex == requestClose) gState.tabRenameIndex = -1;
        CloseWorkspace(gState, requestClose);
        gState.showTabContextMenu = false;
    } else if (requestSwitch >= 0) {
        gState.requestSwitchWorkspace = true;
        gState.requestSwitchWorkspaceIndex = requestSwitch;
    }

}

// draws dropdown under new button with project and tab actions
static void DrawNewMenuDropdown(Rectangle anchor) {
    if (!gState.showNewMenu) return;

    Rectangle menu = { anchor.x, anchor.y + anchor.height + 2.0f, 188.0f, 92.0f };
    DrawRectangleRounded(menu, 0.10f, 6, (Color){ 31, 35, 43, 250 });
    DrawRectangleRoundedLinesEx(menu, 0.10f, 6, 1.2f, (Color){ 96, 108, 126, 255 });

    Rectangle newProjectBtn = { menu.x + 6.0f, menu.y + 6.0f, menu.width - 12.0f, 24.0f };
    Rectangle newTabBtn = { menu.x + 6.0f, menu.y + 34.0f, menu.width - 12.0f, 24.0f };
    Rectangle newPieceTabBtn = { menu.x + 6.0f, menu.y + 62.0f, menu.width - 12.0f, 24.0f };

    if (DrawButton(newProjectBtn, "New Project", "create a new project", false, 15)) {
        gState.showNewMenu = false;
        OpenNewProjectDialog(gState);
    }

    if (DrawButton(newTabBtn, "New Tab", "duplicate active tab", false, 15)) {
        if (gState.activeWorkspaceIndex >= 0 && gState.activeWorkspaceIndex < (int)gState.workspaces.size()) {
            int idx = DuplicateWorkspace(gState, gState.activeWorkspaceIndex, NextWorkspaceName(gState.workspaces[gState.activeWorkspaceIndex].kind));
            if (idx >= 0) {
                gState.requestSwitchWorkspace = true;
                gState.requestSwitchWorkspaceIndex = idx;
            }
        }
        gState.showNewMenu = false;
    }

    if (DrawButton(newPieceTabBtn, "New Piece Tab", "create an empty piece tab", false, 15)) {
        EditorSnapshot snap = {};
        snap.layerNames.push_back("Default");
        snap.layerVisible.push_back(true);
        int idx = AddWorkspace(gState, WorkspaceKind::Piece, NextWorkspaceName(WorkspaceKind::Piece), "", snap);
        if (idx >= 0) {
            gState.requestSwitchWorkspace = true;
            gState.requestSwitchWorkspaceIndex = idx;
        }
        gState.showNewMenu = false;
    }

    bool inside = CheckCollisionPointRec(GetMousePosition(), menu) || CheckCollisionPointRec(GetMousePosition(), anchor);
    if (!inside && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) gState.showNewMenu = false;
}

// handles ui placement and visual composition for all editor sections
void DrawMainGuiLayout(int sw, int sh,
                       float topH, float subTopH,
                       float bodyTop, float statusY, float statusH,
                       const Rectangle &leftIcons,
                       const Rectangle &leftPanel,
                       const Rectangle &viewport,
                       const Rectangle &rightPanel,
                       const Rectangle &statusBar) {
    DrawRectangle(0, 0, sw, sh, (Color){ 19, 22, 28, 255 });

    // draws top command bar with project actions
    DrawRectangle(0, 0, sw, (int)topH, (Color){ 36, 40, 48, 255 });
    DrawLineEx((Vector2){ 0, topH }, (Vector2){ (float)sw, topH }, 1.0f, (Color){ 82, 88, 100, 255 });

    float topX = 14.0f;
    const float topY = 7.0f;
    const float topBtnH = 28.0f;
    const float topGap = 8.0f;
    Rectangle newBtn = { topX, topY, FitButtonWidth("New", 17, 64.0f, 24.0f), topBtnH };
    topX += newBtn.width + topGap;
    Rectangle openBtn = { topX, topY, FitButtonWidth("Open", 17, 64.0f, 24.0f), topBtnH };
    topX += openBtn.width + topGap;
    Rectangle saveBtn = { topX, topY, FitButtonWidth("Save", 17, 64.0f, 24.0f), topBtnH };
    topX += saveBtn.width + topGap;
    Rectangle exportBtn = { topX, topY, FitButtonWidth("Export", 17, 78.0f, 24.0f), topBtnH };
    topX += exportBtn.width + topGap;
    Rectangle importBtn = { topX, topY, FitButtonWidth("Import", 17, 78.0f, 24.0f), topBtnH };
    topX += importBtn.width + topGap;
    Rectangle settingsBtn = { topX, topY, FitButtonWidth("Settings", 17, 90.0f, 24.0f), topBtnH };

    if (DrawButton(newBtn, "New", "new project and tab actions")) gState.showNewMenu = !gState.showNewMenu;
    if (DrawButton(openBtn, "Open", "open project from folder")) {
        gState.showNewMenu = false;
        OpenExistingProjectDialog(gState);
    }
    if (DrawButton(saveBtn, "Save", "save active project or piece")) {
        bool saved = false;
        if (gState.activeWorkspaceIndex >= 0 && gState.activeWorkspaceIndex < (int)gState.workspaces.size() &&
            gState.workspaces[gState.activeWorkspaceIndex].kind == WorkspaceKind::Piece) {
            saved = SaveActivePieceWorkspace(gState);
            if (saved) gState.workspaces[gState.activeWorkspaceIndex].dirty = false;
        } else {
            saved = SaveCurrentProject(gState, false);
        }
        if (!saved) AddLog(gState, "save unavailable open or create a project first");
    }
    if (DrawButton(exportBtn, "Export", "export .vehicle to builds folder")) ExportCurrentVehicle(gState);
    if (DrawButton(importBtn, "Import", "import .vehicle into current workspace")) ImportVehicleFromPicker(gState);
    if (DrawButton(settingsBtn, "Settings", "open editor settings", gState.showSettings)) gState.showSettings = !gState.showSettings;

    int titleX = (int)settingsBtn.x + (int)settingsBtn.width + 18;
    if (sw - titleX > 180) DrawText("Le Bombo Game Engine", titleX, 10, 20, (Color){ 230, 233, 240, 255 });

    if (sw - titleX > 420) {
        char projectTitle[256] = { 0 };
        std::snprintf(projectTitle, sizeof(projectTitle), "project %s", gState.currentProjectName.c_str());
        DrawText(projectTitle, sw - 280, 12, 16, gState.projectDirty ? (Color){ 255, 197, 120, 255 } : (Color){ 179, 187, 201, 255 });
    }

    // draws sub-toolbar with viewport and workflow controls
    DrawRectangle(0, (int)topH, sw, (int)subTopH, (Color){ 31, 35, 42, 255 });
    DrawLineEx((Vector2){ 0, bodyTop }, (Vector2){ (float)sw, bodyTop }, 1.0f, (Color){ 78, 84, 96, 255 });

    bool compact = sw < 1180;
    bool veryCompact = sw < 1030;

    float actionY = topH + 3.0f;
    float actionH = 27.0f;
    float actionX = (float)sw - 8.0f;
    const float gap = 6.0f;

    auto place = [&](float w) {
        actionX -= w;
        Rectangle r = { actionX, actionY, w, actionH };
        actionX -= gap;
        return r;
    };

    Rectangle btnRight = place(24.0f);
    Rectangle btnEye = { 0 };
    if (!compact) btnEye = place(22.0f);
    Rectangle btnPrevBlasts = { 0 };
    if (!compact) btnPrevBlasts = place(FitButtonWidth("Prev Blasts", 15, 104.0f, 24.0f));
    Rectangle btnPreview = place(FitButtonWidth("Preview", 17, 78.0f, 24.0f));
    Rectangle btnRedo = { 0 };
    Rectangle btnUndo = { 0 };
    Rectangle btnMirror = { 0 };
    Rectangle btnMirrorAxis = { 0 };
    if (!veryCompact) {
        btnRedo = place(FitButtonWidth("Redo", 17, 54.0f, 24.0f));
        btnUndo = place(FitButtonWidth("Undo", 17, 54.0f, 24.0f));
        btnMirror = place(FitButtonWidth("Mirror", 17, 72.0f, 24.0f));
        btnMirrorAxis = place(34.0f);
    }
    Rectangle btnSelect = place(FitButtonWidth("Select", 15, 64.0f, 22.0f));
    Rectangle btnScale = place(FitButtonWidth("Scale", 17, 58.0f, 22.0f));
    Rectangle btnRotate = place(FitButtonWidth("Rotate", 17, 62.0f, 22.0f));
    Rectangle btnMove = place(FitButtonWidth("Move", 17, 58.0f, 22.0f));
    Rectangle btn3D = place(FitButtonWidth("3D", 17, 44.0f, 18.0f));
    Rectangle btn2D = place(FitButtonWidth("2D", 17, 44.0f, 18.0f));
    Rectangle btnLeft = place(24.0f);

    float tabsMax = actionX - 10.0f;
    if (tabsMax < 80.0f) tabsMax = 80.0f;
    DrawWorkspaceTabsStrip(8.0f, topH + 3.0f, 27.0f, tabsMax);

    if (DrawButton(btnLeft, gState.showLeftPanel ? "<" : ">", "toggle left tools panel", false, 16)) {
        gState.showLeftPanel = !gState.showLeftPanel;
    }
    if (DrawButton(btn2D, "2D", "switch viewport to 2D mode", gState.viewport2D)) gState.viewport2D = true;
    if (DrawButton(btn3D, "3D", "switch viewport to 3D mode", !gState.viewport2D)) gState.viewport2D = false;
    if (DrawButton(btnMove, "Move", "move gizmo mode (W)", gState.gizmoMode == GizmoMode::Move)) {
        gState.selectMode = false;
        gState.gizmoMode = GizmoMode::Move;
    }
    if (DrawButton(btnRotate, "Rotate", "rotate gizmo mode (E)", gState.gizmoMode == GizmoMode::Rotate)) {
        gState.selectMode = false;
        gState.gizmoMode = GizmoMode::Rotate;
    }
    if (DrawButton(btnScale, "Scale", "scale gizmo mode (R)", gState.gizmoMode == GizmoMode::Scale)) {
        gState.selectMode = false;
        gState.gizmoMode = GizmoMode::Scale;
    }
    if (DrawButton(btnSelect, "Select", "rectangle mode for multi-select", gState.selectMode, 15)) gState.selectMode = !gState.selectMode;

    if (!veryCompact) {
        if (DrawButton(btnMirror, "Mirror", "create mirrored copy of selected blocks on selected axis")) {
            MirrorSelection(gState, gState.mirrorAxis);
        }
        if (DrawButton(btnMirrorAxis, MirrorAxisLabel(gState.mirrorAxis), "toggle mirror axis X Y Z", false, 15)) {
            if (gState.mirrorAxis == MirrorAxis::X) gState.mirrorAxis = MirrorAxis::Y;
            else if (gState.mirrorAxis == MirrorAxis::Y) gState.mirrorAxis = MirrorAxis::Z;
            else gState.mirrorAxis = MirrorAxis::X;
        }
        if (DrawButton(btnUndo, "Undo", "undo latest scene action (Ctrl+Z)")) {
            if (!UndoLastAction(gState)) AddLog(gState, "undo unavailable");
        }
        if (DrawButton(btnRedo, "Redo", "redo latest scene action (Ctrl+Y)")) {
            if (!RedoLastAction(gState)) AddLog(gState, "redo unavailable");
        }
    }

    if (DrawButton(btnPreview, "Preview", "open live preview window")) OpenPreview(gState);
    if (!compact && DrawButton(btnPrevBlasts, "Prev Blasts", "open preview with all shotpoints firing")) OpenBlastPreview(gState, -1);
    if (!compact && DrawButton(btnEye, "o", "show only selected block", gState.isolateSelected, 16)) gState.isolateSelected = !gState.isolateSelected;
    if (DrawButton(btnRight, gState.showRightPanel ? "<" : ">", "toggle inspector panel", false, 16)) gState.showRightPanel = !gState.showRightPanel;

    // draws fixed left icon toolbar when visible
    if (leftIcons.width > 1.0f) {
        DrawRectangleRec(leftIcons, (Color){ 33, 37, 44, 255 });
        DrawRectangleLinesEx(leftIcons, 1.0f, (Color){ 78, 85, 97, 255 });

        float ix = leftIcons.x + 14.0f;
        if (DrawButton((Rectangle){ ix, bodyTop + 16.0f, 40.0f, 40.0f }, "SCN", "scene manager", gState.leftTab == LeftTab::Scene, 14)) gState.leftTab = LeftTab::Scene;
        if (DrawButton((Rectangle){ ix, bodyTop + 66.0f, 40.0f, 40.0f }, "BLK", "blocks palette", gState.leftTab == LeftTab::Blocks, 14)) gState.leftTab = LeftTab::Blocks;
        if (DrawButton((Rectangle){ ix, bodyTop + 116.0f, 40.0f, 40.0f }, "CLR", "color tools", gState.leftTab == LeftTab::Colors, 14)) gState.leftTab = LeftTab::Colors;
        if (DrawButton((Rectangle){ ix, bodyTop + 166.0f, 40.0f, 40.0f }, "PCS", "create and manage piece templates", gState.leftTab == LeftTab::MakePiece, 14)) gState.leftTab = LeftTab::MakePiece;
        if (DrawButton((Rectangle){ ix, bodyTop + 216.0f, 40.0f, 40.0f }, "LSR", "laser shotpoint tools", gState.leftTab == LeftTab::Lasers, 14)) gState.leftTab = LeftTab::Lasers;
    }

    // draws active left panel tab content when visible
    if (leftPanel.width > 1.0f) {
        if (gState.leftTab == LeftTab::Scene) {
            DrawSceneTab(leftPanel, gState);
        } else if (gState.leftTab == LeftTab::Blocks) {
            DrawBlocksTab(leftPanel, gState, viewport);
        } else if (gState.leftTab == LeftTab::Colors) {
            DrawColorPanel(leftPanel, gState);
        } else if (gState.leftTab == LeftTab::MakePiece) {
            DrawMakePieceTab(leftPanel, gState, viewport);
        } else {
            DrawShotpointsTab(leftPanel, gState);
        }
    }

    // draws central viewport in 2d or 3d mode
    if (gState.viewport2D) {
        Draw2DGrid(viewport);
        Draw2DViewportObjects(viewport, gState);
    } else {
        Draw3DViewport(viewport, gState);
    }

    // draws optional right inspector area
    if (rightPanel.width > 1.0f) DrawInspectorPanel(rightPanel, gState);

    // draws bottom status bar and console history output
    DrawRectangleRec(statusBar, (Color){ 29, 33, 39, 255 });
    DrawRectangleLinesEx(statusBar, 1.0f, (Color){ 78, 84, 96, 255 });
    DrawRectangle(0, (int)statusY, sw, 28, (Color){ 38, 42, 50, 255 });
    if (DrawButton((Rectangle){ 8, statusY + 2.0f, 30, 24 }, gState.statusExpanded ? "v" : "^", "expand collapse status panel", false, 16)) {
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

    // draws settings popup over workspace when enabled
    if (gState.showSettings) DrawSettingsPopup((Rectangle){ 14.0f, bodyTop + 10.0f, 340.0f, 352.0f }, gState);

    // draws tab context menu over main layout so it is never hidden by viewport
    DrawWorkspaceTabContextMenu();

    // draws dropdown over main layout so menu is never hidden by viewport
    DrawNewMenuDropdown(newBtn);

    // draws delete confirmation and project dialogs
    DrawDeleteConfirmPopup(gState);
    DrawProjectDialog(gState);

    // draws hover tooltip last
    DrawTooltip();
}
