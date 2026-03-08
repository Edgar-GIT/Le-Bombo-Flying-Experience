#include "../include/workspace_tabs.hpp"
#include "../include/gui_manager.hpp"

#include <algorithm>

// builds one snapshot from current editor state
static EditorSnapshot BuildSnapshotFromState(const EditorGuiState &st) {
    EditorSnapshot snap = {};
    snap.objects = st.objects;
    snap.shotpoints = st.shotpoints;
    snap.layerNames.reserve(st.layers.size());
    snap.layerVisible.reserve(st.layers.size());
    for (const EditorLayer &layer : st.layers) {
        snap.layerNames.push_back(layer.name);
        snap.layerVisible.push_back(layer.visible);
    }
    return snap;
}

// applies one snapshot into current editor state
static void ApplySnapshotToState(EditorGuiState &st, const EditorSnapshot &snap) {
    st.objects = snap.objects;
    st.shotpoints = snap.shotpoints;
    st.layers.clear();

    const int layerCount = (int)std::min(snap.layerNames.size(), snap.layerVisible.size());
    for (int i = 0; i < layerCount; i++) {
        EditorLayer layer = {};
        layer.name = snap.layerNames[i];
        layer.visible = snap.layerVisible[i];
        st.layers.push_back(layer);
    }
    if (st.layers.empty()) {
        EditorLayer def = {};
        def.name = "Default";
        def.visible = true;
        st.layers.push_back(def);
    }

    for (EditorObject &obj : st.objects) {
        if (obj.layerIndex < 0 || obj.layerIndex >= (int)st.layers.size()) obj.layerIndex = 0;
        if (obj.parentIndex < -1 || obj.parentIndex >= (int)st.objects.size()) obj.parentIndex = -1;
    }

    st.selectedIndex = st.objects.empty() ? -1 : 0;
    st.selectedIndices.clear();
    st.selectedShotpoint = st.shotpoints.empty() ? -1 : 0;
    st.activeLayerIndex = 0;
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;
}

// initializes workspace tabs once
void InitWorkspaceTabs(EditorGuiState &st) {
    if (!st.workspaces.empty()) return;

    WorkspaceTab tab = {};
    tab.kind = WorkspaceKind::Project;
    tab.name = st.currentProjectName.empty() ? std::string("project") : st.currentProjectName;
    tab.filePath = st.currentProjectPath;
    tab.snapshot = BuildSnapshotFromState(st);
    tab.dirty = false;
    st.workspaces.push_back(tab);
    st.activeWorkspaceIndex = 0;
}

// syncs the active tab snapshot with current editor state
void SyncActiveWorkspace(EditorGuiState &st) {
    if (st.activeWorkspaceIndex < 0 || st.activeWorkspaceIndex >= (int)st.workspaces.size()) return;
    WorkspaceTab &tab = st.workspaces[st.activeWorkspaceIndex];
    tab.snapshot = BuildSnapshotFromState(st);
    if (tab.kind == WorkspaceKind::Project) {
        tab.name = st.currentProjectName.empty() ? std::string("project") : st.currentProjectName;
        tab.filePath = st.currentProjectPath;
    }
}

// switches current editor view to another workspace tab
bool SwitchWorkspace(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.workspaces.size()) return false;
    if (index == st.activeWorkspaceIndex) return true;

    SyncActiveWorkspace(st);
    st.activeWorkspaceIndex = index;
    ApplySnapshotToState(st, st.workspaces[index].snapshot);
    return true;
}

// resets tabs to the active project workspace only
void ResetWorkspacesForProject(EditorGuiState &st) {
    WorkspaceTab tab = {};
    tab.kind = WorkspaceKind::Project;
    tab.name = st.currentProjectName.empty() ? std::string("project") : st.currentProjectName;
    tab.filePath = st.currentProjectPath;
    tab.snapshot = BuildSnapshotFromState(st);
    tab.dirty = false;
    st.workspaces.clear();
    st.workspaces.push_back(tab);
    st.activeWorkspaceIndex = 0;
}

// finds existing workspace tab by file path
int FindWorkspaceByPath(const EditorGuiState &st, const std::string &filePath) {
    for (int i = 0; i < (int)st.workspaces.size(); i++) {
        if (st.workspaces[i].filePath == filePath && !filePath.empty()) return i;
    }
    return -1;
}

// appends one new workspace tab and returns its index
int AddWorkspace(EditorGuiState &st, WorkspaceKind kind, const std::string &name, const std::string &filePath, const EditorSnapshot &snapshot) {
    WorkspaceTab tab = {};
    tab.kind = kind;
    tab.name = name.empty() ? std::string("tab") : name;
    tab.filePath = filePath;
    tab.snapshot = snapshot;
    tab.dirty = false;
    st.workspaces.push_back(tab);
    return (int)st.workspaces.size() - 1;
}

// closes one workspace tab and keeps editor state consistent
bool CloseWorkspace(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.workspaces.size()) return false;
    if (st.workspaces.size() <= 1) return false;

    SyncActiveWorkspace(st);
    const int active = st.activeWorkspaceIndex;
    st.workspaces.erase(st.workspaces.begin() + index);

    if (active == index) {
        int next = index;
        if (next >= (int)st.workspaces.size()) next = (int)st.workspaces.size() - 1;
        st.activeWorkspaceIndex = next;
        ApplySnapshotToState(st, st.workspaces[next].snapshot);
    } else if (active > index) {
        st.activeWorkspaceIndex = active - 1;
    }

    return true;
}

// renames one workspace tab label
bool RenameWorkspace(EditorGuiState &st, int index, const std::string &name) {
    if (index < 0 || index >= (int)st.workspaces.size()) return false;
    if (name.empty()) return false;
    st.workspaces[index].name = name;
    return true;
}

// duplicates one existing workspace snapshot into a new tab
int DuplicateWorkspace(EditorGuiState &st, int index, const std::string &name) {
    if (index < 0 || index >= (int)st.workspaces.size()) return -1;
    SyncActiveWorkspace(st);
    WorkspaceTab src = st.workspaces[index];
    src.name = name.empty() ? (src.name + "_copy") : name;
    src.filePath.clear();
    src.dirty = true;
    st.workspaces.push_back(src);
    return (int)st.workspaces.size() - 1;
}
