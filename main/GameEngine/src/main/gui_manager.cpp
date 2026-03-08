#include "../include/gui_manager.hpp"
#include "../include/gui_shotpoints.hpp"
#include "../include/gui_workflow.hpp"
#include "../include/main_gui.hpp"
#include "../include/piece_library.hpp"
#include "../include/persistence.hpp"
#include "../include/workspace_tabs.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

// stores global editor state shared across gui modules
EditorGuiState gState = {};
// stores the active hover tooltip text for the current frame
const char *gHoverTooltip = nullptr;
// blocks interactive controls while modal or text input is active
bool gBlockUiInput = false;

// returns a bounded float value between the provided limits
float Clampf(float v, float minV, float maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

// interpolates linearly between two float values
float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

// formats the local timestamp used in logs and cache entries
static void BuildTimestamp(char out[32]) {
    std::time_t now = std::time(nullptr);
    std::tm tmNow = {};
#if defined(_WIN32)
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    std::strftime(out, 32, "%Y-%m-%d %H:%M:%S", &tmNow);
}

// text escaping
static std::string EscapeJson(const std::string &text) {
    std::string out;
    out.reserve(text.size() + 16);
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

// root resolving
static std::filesystem::path ResolveProjectRootPath() {
    std::filesystem::path current = std::filesystem::current_path();
    while (true) {
        if (std::filesystem::exists(current / "main" / "src" / "main.c")) {
            return current;
        }
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return std::filesystem::current_path();
}

// cache file system
static std::filesystem::path ResolveSessionCachePath() {
    const std::filesystem::path root = ResolveProjectRootPath();
    const std::filesystem::path cacheDir = root / "main" / "GameEngine" / "cache";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);
    return cacheDir / "engine_session_cache.json";
}

// scene and shotpoint changes
static std::string BuildSceneFingerprint(const std::vector<EditorObject> &objects,
                                         const std::vector<Shotpoint> &shotpoints,
                                         const std::vector<EditorLayer> &layers) {
    std::string out;
    out.reserve(objects.size() * 140 + shotpoints.size() * 100 + layers.size() * 40 + 64);
    char buf[256] = { 0 };

    std::snprintf(buf, sizeof(buf), "obj:%zu|sp:%zu|ly:%zu|", objects.size(), shotpoints.size(), layers.size());
    out += buf;

    for (const EditorObject &obj : objects) {
        std::snprintf(buf, sizeof(buf),
                      "%s|%d|%d|%d|%d|%d|%d|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%u|%u|%u|%u;",
                      obj.name.c_str(),
                      (int)obj.kind,
                      obj.is2D ? 1 : 0,
                      obj.visible ? 1 : 0,
                      obj.anchored ? 1 : 0,
                      obj.layerIndex,
                      obj.parentIndex,
                      obj.position.x, obj.position.y, obj.position.z,
                      obj.rotation.x, obj.rotation.y, obj.rotation.z,
                      obj.scale.x, obj.scale.y, obj.scale.z,
                      obj.color.r, obj.color.g, obj.color.b, obj.color.a);
        out += buf;
    }

    for (const Shotpoint &sp : shotpoints) {
        std::snprintf(buf, sizeof(buf),
                      "%s|%d|%d|%.4f|%.4f|%.4f|%u|%u|%u|%u|%.4f;",
                      sp.name.c_str(),
                      sp.objectIndex,
                      sp.enabled ? 1 : 0,
                      sp.localPos.x, sp.localPos.y, sp.localPos.z,
                      sp.blastColor.r, sp.blastColor.g, sp.blastColor.b, sp.blastColor.a,
                      sp.blastSize);
        out += buf;
    }

    for (const EditorLayer &layer : layers) {
        std::snprintf(buf, sizeof(buf), "%s|%d;", layer.name.c_str(), layer.visible ? 1 : 0);
        out += buf;
    }

    return out;
}

// ensures selection indexes remain valid after scene restores
static void ClampSelectionIndexes(EditorGuiState &st) {
    const int count = (int)st.objects.size();
    if (count <= 0) {
        st.selectedIndex = -1;
        st.selectedIndices.clear();
        st.renameIndex = -1;
        st.renameFromInspector = false;
        st.colorSyncIndex = -1;
    } else {
        if (st.selectedIndex >= count) st.selectedIndex = count - 1;
        if (st.selectedIndex < -1) st.selectedIndex = -1;
        if (st.renameIndex >= count) {
            st.renameIndex = -1;
            st.renameFromInspector = false;
        }

        std::vector<int> filtered;
        filtered.reserve(st.selectedIndices.size());
        for (int idx : st.selectedIndices) {
            if (idx >= 0 && idx < count) filtered.push_back(idx);
        }
        std::sort(filtered.begin(), filtered.end());
        filtered.erase(std::unique(filtered.begin(), filtered.end()), filtered.end());
        st.selectedIndices = filtered;
        if (st.selectedIndex < 0 && !st.selectedIndices.empty()) st.selectedIndex = st.selectedIndices[0];
    }

    if (st.layers.empty()) {
        EditorLayer def = {};
        def.name = "Default";
        def.visible = true;
        st.layers.push_back(def);
    }
    if (st.activeLayerIndex < 0 || st.activeLayerIndex >= (int)st.layers.size()) st.activeLayerIndex = 0;

    if (!st.shotpoints.empty()) {
        if (st.selectedShotpoint < 0) st.selectedShotpoint = 0;
        if (st.selectedShotpoint >= (int)st.shotpoints.size()) st.selectedShotpoint = (int)st.shotpoints.size() - 1;
    } else {
        st.selectedShotpoint = -1;
    }
}

// returns layer visibility for one object
static bool IsObjectLayerVisible(const EditorGuiState &st, const EditorObject &obj) {
    if (obj.layerIndex < 0 || obj.layerIndex >= (int)st.layers.size()) return true;
    return st.layers[obj.layerIndex].visible;
}

// returns true when current active tab is the project workspace
static bool IsProjectWorkspaceActive(const EditorGuiState &st) {
    if (st.activeWorkspaceIndex < 0 || st.activeWorkspaceIndex >= (int)st.workspaces.size()) return true;
    return st.workspaces[st.activeWorkspaceIndex].kind == WorkspaceKind::Project;
}

// finds target layer index by name or creates one in destination snapshot
static int ResolveSnapshotLayer(EditorSnapshot &snap, const std::string &name, bool visible) {
    for (int i = 0; i < (int)snap.layerNames.size(); i++) {
        if (snap.layerNames[i] == name) return i;
    }
    snap.layerNames.push_back(name);
    snap.layerVisible.push_back(visible);
    return (int)snap.layerNames.size() - 1;
}

// copies current selection into another workspace tab
static bool CopySelectionToWorkspace(EditorGuiState &st, int targetIndex) {
    if (targetIndex < 0 || targetIndex >= (int)st.workspaces.size()) return false;
    if (targetIndex == st.activeWorkspaceIndex) return false;

    std::vector<int> indices = st.selectedIndices;
    if (indices.empty() && st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
        indices.push_back(st.selectedIndex);
    }
    if (indices.empty()) return false;

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    SyncActiveWorkspace(st);
    WorkspaceTab &target = st.workspaces[targetIndex];
    EditorSnapshot &dst = target.snapshot;
    if (dst.layerNames.empty()) {
        dst.layerNames.push_back("Default");
        dst.layerVisible.push_back(true);
    }

    const int base = (int)dst.objects.size();
    std::unordered_map<int, int> remap;
    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        EditorObject obj = st.objects[idx];
        std::string layerName = "Default";
        bool layerVisible = true;
        if (obj.layerIndex >= 0 && obj.layerIndex < (int)st.layers.size()) {
            layerName = st.layers[obj.layerIndex].name;
            layerVisible = st.layers[obj.layerIndex].visible;
        }
        obj.layerIndex = ResolveSnapshotLayer(dst, layerName, layerVisible);
        obj.parentIndex = -1;
        remap[idx] = (int)dst.objects.size();
        dst.objects.push_back(obj);
    }
    if (dst.objects.size() == (size_t)base) return false;

    for (const Shotpoint &sp : st.shotpoints) {
        auto it = remap.find(sp.objectIndex);
        if (it == remap.end()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = it->second;
        dst.shotpoints.push_back(copy);
    }

    target.dirty = true;
    char msg[196] = { 0 };
    std::snprintf(msg, sizeof(msg), "copied %d object(s) to tab %s", (int)(dst.objects.size() - base), target.name.c_str());
    AddLog(st, msg);
    RecordCacheAction(st, "workspace_copy", msg);
    return true;
}

// builds one sorted unique index list from current selected objects
static std::vector<int> BuildSelectionIndices(const EditorGuiState &st) {
    std::vector<int> indices = st.selectedIndices;
    if (indices.empty() && st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
        indices.push_back(st.selectedIndex);
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

// copies selected objects and related shotpoints into clipboard buffers
static bool CopySelectionToClipboard(EditorGuiState &st) {
    std::vector<int> indices = BuildSelectionIndices(st);
    if (indices.empty()) {
        AddLog(st, "clipboard copy ignored no block selected");
        return false;
    }

    st.clipboardObjects.clear();
    st.clipboardShotpoints.clear();

    Vector3 center = { 0.0f, 0.0f, 0.0f };
    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        center = Vector3Add(center, st.objects[idx].position);
    }
    if (!indices.empty()) {
        float inv = 1.0f / (float)indices.size();
        center = Vector3Scale(center, inv);
    }

    std::unordered_map<int, int> remap;
    st.clipboardObjects.reserve(indices.size());
    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        EditorObject copy = st.objects[idx];
        copy.position = Vector3Subtract(copy.position, center);
        remap[idx] = (int)st.clipboardObjects.size();
        st.clipboardObjects.push_back(copy);
    }

    for (size_t i = 0; i < indices.size(); i++) {
        int srcIdx = indices[i];
        if (srcIdx < 0 || srcIdx >= (int)st.objects.size() || i >= st.clipboardObjects.size()) continue;
        int srcParent = st.objects[srcIdx].parentIndex;
        auto itParent = remap.find(srcParent);
        st.clipboardObjects[i].parentIndex = (itParent != remap.end()) ? itParent->second : -1;
    }

    for (const Shotpoint &sp : st.shotpoints) {
        auto it = remap.find(sp.objectIndex);
        if (it == remap.end()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = it->second;
        st.clipboardShotpoints.push_back(copy);
    }

    st.clipboardPasteCount = 0;
    char msg[196] = { 0 };
    std::snprintf(msg, sizeof(msg), "copied %d object(s) to clipboard", (int)st.clipboardObjects.size());
    AddLog(st, msg);
    RecordCacheAction(st, "clipboard_copy", msg);
    return !st.clipboardObjects.empty();
}

// pastes clipboard objects near selection or camera target and selects new objects
static bool PasteClipboard(EditorGuiState &st) {
    if (st.clipboardObjects.empty()) {
        AddLog(st, "clipboard paste ignored clipboard is empty");
        return false;
    }

    int base = (int)st.objects.size();
    Vector3 anchor = st.orbitTarget;
    if (st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
        anchor = st.objects[st.selectedIndex].position;
    }
    float step = 1.25f * (float)(st.clipboardPasteCount + 1);
    Vector3 offset = { step, 0.0f, step };

    st.selectedIndices.clear();
    st.objects.reserve(st.objects.size() + st.clipboardObjects.size());
    for (size_t i = 0; i < st.clipboardObjects.size(); i++) {
        EditorObject copy = st.clipboardObjects[i];
        copy.position = Vector3Add(copy.position, Vector3Add(anchor, offset));
        copy.parentIndex = (copy.parentIndex >= 0 && copy.parentIndex < (int)st.clipboardObjects.size()) ? (base + copy.parentIndex) : -1;
        if (copy.layerIndex < 0 || copy.layerIndex >= (int)st.layers.size()) {
            copy.layerIndex = (st.activeLayerIndex >= 0 && st.activeLayerIndex < (int)st.layers.size()) ? st.activeLayerIndex : 0;
        }
        st.objects.push_back(copy);
        st.selectedIndices.push_back((int)st.objects.size() - 1);
    }

    for (const Shotpoint &sp : st.clipboardShotpoints) {
        if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.clipboardObjects.size()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = base + sp.objectIndex;
        st.shotpoints.push_back(copy);
    }

    st.selectedIndex = st.selectedIndices.empty() ? -1 : st.selectedIndices[0];
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;
    st.clipboardPasteCount++;

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    MarkProjectDirty(st);

    char msg[196] = { 0 };
    std::snprintf(msg, sizeof(msg), "pasted %d object(s) from clipboard", (int)st.clipboardObjects.size());
    AddLog(st, msg);
    RecordCacheAction(st, "clipboard_paste", msg);
    return true;
}

// deletes one object and rewires parents shotpoints and selection indexes
static void RemoveObjectAt(EditorGuiState &st, int index) {
    if (index < 0 || index >= (int)st.objects.size()) return;
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

    if (st.selectedIndex == index) st.selectedIndex = -1;
    else if (st.selectedIndex > index) st.selectedIndex--;
}

// duplicates the selected objects and associated shotpoints with small offset
static bool DuplicateSelection(EditorGuiState &st) {
    std::vector<int> indices = BuildSelectionIndices(st);
    if (indices.empty()) {
        AddLog(st, "duplicate ignored no block selected");
        return false;
    }

    std::unordered_map<int, int> remap;
    std::vector<int> newSelection;
    newSelection.reserve(indices.size());

    Vector3 offset = { 1.25f, 0.0f, 1.25f };

    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        EditorObject copy = st.objects[idx];
        copy.name += "_copy";
        copy.position = Vector3Add(copy.position, offset);
        int newIdx = (int)st.objects.size();
        remap[idx] = newIdx;
        st.objects.push_back(copy);
        newSelection.push_back(newIdx);
    }

    for (size_t i = 0; i < indices.size(); i++) {
        int srcIdx = indices[i];
        auto itCopy = remap.find(srcIdx);
        if (itCopy == remap.end()) continue;
        int dstIdx = itCopy->second;
        if (dstIdx < 0 || dstIdx >= (int)st.objects.size()) continue;
        int srcParent = st.objects[srcIdx].parentIndex;
        auto itParent = remap.find(srcParent);
        st.objects[dstIdx].parentIndex = (itParent != remap.end()) ? itParent->second : srcParent;
    }

    for (const Shotpoint &sp : st.shotpoints) {
        auto it = remap.find(sp.objectIndex);
        if (it == remap.end()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = it->second;
        st.shotpoints.push_back(copy);
    }

    st.selectedIndices = newSelection;
    st.selectedIndex = st.selectedIndices.empty() ? -1 : st.selectedIndices[0];
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    MarkProjectDirty(st);

    char msg[192] = { 0 };
    std::snprintf(msg, sizeof(msg), "duplicated %d block(s)", (int)newSelection.size());
    AddLog(st, msg);
    RecordCacheAction(st, "duplicate", msg);
    return !newSelection.empty();
}

// cuts selected objects into clipboard and removes them from scene
static bool CutSelection(EditorGuiState &st) {
    std::vector<int> indices = BuildSelectionIndices(st);
    if (indices.empty()) {
        AddLog(st, "cut ignored no block selected");
        return false;
    }
    if (!CopySelectionToClipboard(st)) return false;

    for (int i = (int)indices.size() - 1; i >= 0; i--) {
        RemoveObjectAt(st, indices[i]);
    }

    st.selectedIndices.clear();
    st.selectedIndex = -1;
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    MarkProjectDirty(st);

    char msg[192] = { 0 };
    std::snprintf(msg, sizeof(msg), "cut %d block(s) to clipboard", (int)indices.size());
    AddLog(st, msg);
    RecordCacheAction(st, "cut", msg);
    return true;
}

// mirrors selected objects into new mirrored copies around selection center
bool MirrorSelection(EditorGuiState &st, MirrorAxis axis) {
    std::vector<int> indices = BuildSelectionIndices(st);
    if (indices.empty()) {
        AddLog(st, "mirror ignored no block selected");
        return false;
    }

    Vector3 center = { 0.0f, 0.0f, 0.0f };
    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        center = Vector3Add(center, st.objects[idx].position);
    }
    center = Vector3Scale(center, 1.0f / (float)indices.size());

    std::unordered_map<int, int> remap;
    std::vector<int> newSelection;
    newSelection.reserve(indices.size());

    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        EditorObject copy = st.objects[idx];
        copy.name += "_mir";

        Vector3 local = Vector3Subtract(copy.position, center);
        if (axis == MirrorAxis::X) {
            local.x = -local.x;
            copy.rotation.y = -copy.rotation.y;
            copy.rotation.z = -copy.rotation.z;
        } else if (axis == MirrorAxis::Y) {
            local.y = -local.y;
            copy.rotation.x = -copy.rotation.x;
            copy.rotation.z = -copy.rotation.z;
        } else {
            local.z = -local.z;
            copy.rotation.x = -copy.rotation.x;
            copy.rotation.y = -copy.rotation.y;
        }
        copy.position = Vector3Add(center, local);

        int newIdx = (int)st.objects.size();
        remap[idx] = newIdx;
        st.objects.push_back(copy);
        newSelection.push_back(newIdx);
    }

    for (int srcIdx : indices) {
        auto itCopy = remap.find(srcIdx);
        if (itCopy == remap.end()) continue;
        int dstIdx = itCopy->second;
        if (dstIdx < 0 || dstIdx >= (int)st.objects.size()) continue;
        int srcParent = st.objects[srcIdx].parentIndex;
        auto itParent = remap.find(srcParent);
        st.objects[dstIdx].parentIndex = (itParent != remap.end()) ? itParent->second : srcParent;
    }

    for (const Shotpoint &sp : st.shotpoints) {
        auto it = remap.find(sp.objectIndex);
        if (it == remap.end()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = it->second;
        copy.localPos = (axis == MirrorAxis::X) ? (Vector3){ -copy.localPos.x, copy.localPos.y, copy.localPos.z } :
                        (axis == MirrorAxis::Y) ? (Vector3){ copy.localPos.x, -copy.localPos.y, copy.localPos.z } :
                                                  (Vector3){ copy.localPos.x, copy.localPos.y, -copy.localPos.z };
        st.shotpoints.push_back(copy);
    }

    st.selectedIndices = newSelection;
    st.selectedIndex = st.selectedIndices.empty() ? -1 : st.selectedIndices[0];
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    MarkProjectDirty(st);

    const char *axisName = (axis == MirrorAxis::X) ? "X" : (axis == MirrorAxis::Y) ? "Y" : "Z";
    char msg[192] = { 0 };
    std::snprintf(msg, sizeof(msg), "mirrored %d block(s) on %s axis", (int)newSelection.size(), axisName);
    AddLog(st, msg);
    RecordCacheAction(st, "mirror", msg);
    return !newSelection.empty();
}

// writes the current in-memory action list to the temporary json cache file
void FlushSessionCache(EditorGuiState &st) {
    if (!st.cacheInitialized || st.cacheFilePath.empty()) return;

    std::ofstream out(st.cacheFilePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"actions\": [\n";
    for (size_t i = 0; i < st.cacheActions.size(); i++) {
        const CacheActionEntry &entry = st.cacheActions[i];
        out << "    {\"timestamp\":\"" << EscapeJson(entry.timestamp)
            << "\",\"action\":\"" << EscapeJson(entry.action)
            << "\",\"detail\":\"" << EscapeJson(entry.detail) << "\"}";
        if (i + 1 < st.cacheActions.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

// registers one user action entry and syncs it to the cache file
void RecordCacheAction(EditorGuiState &st, const char *action, const char *detail) {
    if (!st.cacheInitialized) {
        const std::filesystem::path cachePath = ResolveSessionCachePath();
        st.cacheFilePath = cachePath.string();
        st.cacheInitialized = true;
    }

    CacheActionEntry entry = {};
    BuildTimestamp(entry.timestamp);
    entry.action = action ? action : "action";
    entry.detail = detail ? detail : "";
    st.cacheActions.push_back(entry);

    if ((int)st.cacheActions.size() > kUserConfig.maxCacheEntries) {
        const int trim = (int)st.cacheActions.size() - kUserConfig.maxCacheEntries;
        st.cacheActions.erase(st.cacheActions.begin(), st.cacheActions.begin() + trim);
    }

    FlushSessionCache(st);
}

// deletes the temporary cache file used by the current editor session
static void RemoveSessionCacheFile(EditorGuiState &st) {
    if (!st.cacheInitialized || st.cacheFilePath.empty()) return;
    std::error_code ec;
    std::filesystem::remove(st.cacheFilePath, ec);
    st.cacheInitialized = false;
    st.cacheFilePath.clear();
    st.cacheActions.clear();
}

// sets hover tooltip text only when the cursor is over a control
void SetHoverTooltip(bool hover, const char *text) {
    if (hover && text != nullptr) gHoverTooltip = text;
}

// adds a console/status log line and mirrors it into session cache
void AddLog(EditorGuiState &st, const char *message) {
    EngineLogEntry entry = {};
    BuildTimestamp(entry.timestamp);
    entry.text = message ? message : "";
    st.logs.push_back(entry);
    if ((int)st.logs.size() > kUserConfig.maxLogEntries) {
        st.logs.erase(st.logs.begin(), st.logs.begin() + kUserConfig.trimLogEntries);
    }
    std::printf("[%s] %s\n", entry.timestamp, entry.text.c_str());
    RecordCacheAction(st, "log", entry.text.c_str());
}

// updates text input buffers used by inline rename controls
void UpdateTextFieldBuffer(char *buffer, size_t bufferSize, bool *submit, bool *cancel) {
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

// returns the primitive definition metadata for a specific primitive kind
const PrimitiveDef *FindPrimitiveDef(PrimitiveKind kind) {
    for (int i = 0; i < kPrimitiveDefCount; i++) {
        const PrimitiveDef &def = kPrimitiveDefs[i];
        if (def.kind == kind) return &def;
    }
    return nullptr;
}

// checks whether a primitive should be shown with the active filter
bool PrimitivePassesFilter(const PrimitiveDef &def, FilterMode mode) {
    if (mode == FilterMode::All) return true;
    if (mode == FilterMode::TwoD) return def.is2D;
    return !def.is2D;
}

// draws a standard editor button and returns true on click
bool DrawButton(Rectangle rect, const char *label, const char *tooltip, bool active, int fontSize) {
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

// computes the distance from a point to a 2d segment for gizmo axis tests
float DistancePointToSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = Vector2Subtract(b, a);
    float abLenSq = ab.x * ab.x + ab.y * ab.y;
    if (abLenSq <= 0.00001f) return Vector2Distance(p, a);
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / abLenSq;
    t = Clampf(t, 0.0f, 1.0f);
    Vector2 proj = { a.x + ab.x * t, a.y + ab.y * t };
    return Vector2Distance(p, proj);
}

// converts rgb color to hsv float triplet
Vector3 ColorToHSVf(Color c) {
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

// captures current objects and shotpoints into one snapshot
static EditorSnapshot BuildSnapshot(const EditorGuiState &st) {
    EditorSnapshot snap = {};
    snap.objects = st.objects;
    snap.shotpoints = st.shotpoints;
    for (const EditorLayer &layer : st.layers) {
        snap.layerNames.push_back(layer.name);
        snap.layerVisible.push_back(layer.visible);
    }
    return snap;
}

// applies scene restore from history and re-synchronizes tracking state
static void ApplyHistoryScene(EditorGuiState &st, const EditorSnapshot &scene) {
    st.historyApplying = true;
    st.objects = scene.objects;
    st.shotpoints = scene.shotpoints;
    st.layers.clear();
    const size_t layerCount = std::min(scene.layerNames.size(), scene.layerVisible.size());
    for (size_t i = 0; i < layerCount; i++) {
        EditorLayer layer = {};
        layer.name = scene.layerNames[i];
        layer.visible = scene.layerVisible[i];
        st.layers.push_back(layer);
    }
    if (st.layers.empty()) {
        EditorLayer def = {};
        def.name = "Default";
        def.visible = true;
        st.layers.push_back(def);
    }
    st.historyApplying = false;
    ResetHistoryBaseline(st);
    ClampSelectionIndexes(st);
}

// reinicia a baseline do historico para o estado atual da cena
void ResetHistoryBaseline(EditorGuiState &st) {
    st.historyPending = false;
    st.historyPendingBeforeSnapshot = {};
    st.historyBaselineSnapshot = BuildSnapshot(st);
    st.historyBaselineHash = BuildSceneFingerprint(st.objects, st.shotpoints, st.layers);
}

// executes one undo step if available and updates redo history
bool UndoLastAction(EditorGuiState &st) {
    if (st.undoStack.empty()) return false;

    st.redoStack.push_back(BuildSnapshot(st));
    if ((int)st.redoStack.size() > kUserConfig.maxUndoEntries) {
        st.redoStack.erase(st.redoStack.begin());
    }

    const EditorSnapshot scene = st.undoStack.back();
    st.undoStack.pop_back();
    ApplyHistoryScene(st, scene);
    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    if (IsProjectWorkspaceActive(st)) MarkProjectDirty(st);
    AddLog(st, "Undo");
    RecordCacheAction(st, "undo", "Undo scene change");
    return true;
}

// executes one redo step if available and updates undo history
bool RedoLastAction(EditorGuiState &st) {
    if (st.redoStack.empty()) return false;

    st.undoStack.push_back(BuildSnapshot(st));
    if ((int)st.undoStack.size() > kUserConfig.maxUndoEntries) {
        st.undoStack.erase(st.undoStack.begin());
    }

    const EditorSnapshot scene = st.redoStack.back();
    st.redoStack.pop_back();
    ApplyHistoryScene(st, scene);
    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    if (IsProjectWorkspaceActive(st)) MarkProjectDirty(st);
    AddLog(st, "Redo");
    RecordCacheAction(st, "redo", "Redo scene change");
    return true;
}

// captures stable scene changes and pushes them into undo history
static void UpdateHistoryTracker(EditorGuiState &st) {
    const std::string currentHash = BuildSceneFingerprint(st.objects, st.shotpoints, st.layers);

    if (!st.historyPending && currentHash != st.historyBaselineHash) {
        st.historyPending = true;
        st.historyLastChangeTime = GetTime();
        st.historyPendingBeforeSnapshot = st.historyBaselineSnapshot;
    } else if (st.historyPending && currentHash != st.historyBaselineHash) {
        st.historyLastChangeTime = GetTime();
    }

    if (!st.historyPending) return;

    bool mouseBusy = IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                     IsMouseButtonDown(MOUSE_RIGHT_BUTTON) ||
                     IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
    double now = GetTime();
    bool stabilizedByTime = (now - st.historyLastChangeTime) > 0.18;
    bool stabilizedByInput = !mouseBusy;
    if (!stabilizedByTime && !stabilizedByInput) return;

    std::vector<EditorLayer> beforeLayers;
    const size_t beforeCount = std::min(st.historyPendingBeforeSnapshot.layerNames.size(),
                                        st.historyPendingBeforeSnapshot.layerVisible.size());
    beforeLayers.reserve(beforeCount);
    for (size_t i = 0; i < beforeCount; i++) {
        EditorLayer layer = {};
        layer.name = st.historyPendingBeforeSnapshot.layerNames[i];
        layer.visible = st.historyPendingBeforeSnapshot.layerVisible[i];
        beforeLayers.push_back(layer);
    }
    const std::string beforeHash = BuildSceneFingerprint(st.historyPendingBeforeSnapshot.objects,
                                                         st.historyPendingBeforeSnapshot.shotpoints,
                                                         beforeLayers);
    const std::string finalHash = BuildSceneFingerprint(st.objects, st.shotpoints, st.layers);

    if (!st.historyApplying && beforeHash != finalHash) {
        st.undoStack.push_back(st.historyPendingBeforeSnapshot);
        if ((int)st.undoStack.size() > kUserConfig.maxUndoEntries) {
            st.undoStack.erase(st.undoStack.begin());
        }
        st.redoStack.clear();
        if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
            st.workspaces[st.activeWorkspaceIndex].dirty = true;
        }
        if (IsProjectWorkspaceActive(st)) MarkProjectDirty(st);
        RecordCacheAction(st, "scene_change", "Scene modified");
    }

    ResetHistoryBaseline(st);
}

// initializes editor state defaults and session cache tracking
static void InitState() {
    if (gState.initialized) return;

    gState.initialized = true;
    gState.showLeftPanel = true;
    gState.showRightPanel = true;
    gState.showSettings = false;
    gState.statusExpanded = false;
    gState.isolateSelected = false;
    gState.selectMode = false;
    gState.boxSelecting = false;
    gState.viewport2D = false;
    gState.previewOpen = false;
    gState.shotpointPlacementOpen = false;
    gState.leftTab = LeftTab::Scene;
    gState.filterMode = FilterMode::All;
    gState.gizmoMode = GizmoMode::Move;
    gState.activeAxis = GizmoAxis::None;
    gState.previewMode = PreviewMode::Scene;
    gState.leftPanelT = 1.0f;
    gState.rightPanelT = 1.0f;
    gState.statusPanelT = 0.0f;
    gState.boxSelectStart = { 0.0f, 0.0f };
    gState.boxSelectEnd = { 0.0f, 0.0f };
    gState.selectedIndices.clear();
    gState.selectionDragActive = false;
    gState.selectionDragStart = { 0.0f, 0.0f };

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
    gState.previewShotpointIndex = -1;
    gState.previewBlastTimer = 0.0f;

    gState.shotpointPlacementCamera = {};
    gState.shotpointPlacementCamera.fovy = 60.0f;
    gState.shotpointPlacementCamera.projection = CAMERA_PERSPECTIVE;
    gState.shotpointPlacementYaw = 0.0f;
    gState.shotpointPlacementPitch = 0.0f;
    gState.shotpointPlacementIndex = -1;

    gState.selectedIndex = -1;
    gState.activeLayerIndex = 0;
    gState.selectedShotpoint = -1;
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
    gState.piecesScroll = 0.0f;
    gState.sceneScroll = 0.0f;
    gState.layerScroll = 0.0f;
    gState.showLayerPanel = true;
    gState.showNewMenu = false;
    gState.showTabContextMenu = false;
    gState.tabContextIndex = -1;
    gState.tabContextPos = { 0.0f, 0.0f };
    gState.tabRenameIndex = -1;
    gState.tabRenameBuffer[0] = '\0';

    gState.colorHue = 0.0f;
    gState.colorSat = 1.0f;
    gState.colorVal = 1.0f;
    gState.colorSyncIndex = -1;
    gState.vehicleForwardYawDeg = 0.0f;
    gState.mirrorAxis = MirrorAxis::X;

    gState.shotpointPlacementAxis = GizmoAxis::None;
    gState.shotpointPlacementDrag = false;

    gState.historyPending = false;
    gState.historyApplying = false;
    gState.historyLastChangeTime = 0.0;
    gState.historyBaselineSnapshot = {};
    gState.historyPendingBeforeSnapshot = {};
    gState.undoStack.clear();
    gState.redoStack.clear();

    gState.cacheInitialized = false;
    gState.cacheFilePath.clear();
    gState.cacheActions.clear();
    gState.showProjectDialog = false;
    gState.projectDialogMode = ProjectDialogMode::None;
    gState.showUnsavedConfirm = false;
    gState.pendingDialogMode = ProjectDialogMode::None;
    gState.showRecoveryDialog = false;
    gState.recoveryProjectPath.clear();
    gState.projectNameBuffer[0] = '\0';
    gState.projectErrorBuffer[0] = '\0';
    gState.projectListScroll = 0.0f;
    gState.projectFolderList.clear();
    gState.projectsRootPath.clear();
    gState.currentProjectPath.clear();
    gState.currentProjectName.clear();
    gState.piecesRootPath.clear();
    gState.projectLoaded = false;
    gState.projectDirty = false;
    gState.autosaveIntervalSec = 4.0;
    gState.autosaveLastTime = 0.0;

    gState.layers.clear();
    EditorLayer defaultLayer = {};
    defaultLayer.name = "Default";
    defaultLayer.visible = true;
    gState.layers.push_back(defaultLayer);
    gState.pieceLibrary.clear();
    gState.clipboardObjects.clear();
    gState.clipboardShotpoints.clear();
    gState.clipboardPasteCount = 0;
    gState.workspaces.clear();
    gState.activeWorkspaceIndex = -1;
    gState.workspaceTabRects.clear();
    gState.hoveredWorkspaceTab = -1;
    gState.requestSwitchWorkspace = false;
    gState.requestSwitchWorkspaceIndex = -1;

    UpdateCameraFromOrbit(gState);
    ResetHistoryBaseline(gState);
    InitProjectPersistence(gState);
    InitPieceLibrary(gState);
    InitWorkspaceTabs(gState);

    RecordCacheAction(gState, "session", "Engine editor session started");
    AddLog(gState, "Engine editor ready");
}

// processes input updates state and draws all editor ui modules
void DrawEngineGuiLayout(float dt) {
    InitState();
    gHoverTooltip = nullptr;
    gBlockUiInput = false;

    if (gState.previewOpen) {
        DrawPreviewWindow(gState, dt);
        DrawTooltip();
        UpdateHistoryTracker(gState);
        return;
    }

    if (gState.shotpointPlacementOpen) {
        DrawShotpointPlacementWindow(gState, dt);
        DrawTooltip();
        UpdateHistoryTracker(gState);
        return;
    }

    float t = Clampf(dt * kUserConfig.uiAnimationSpeed, 0.0f, 1.0f);
    gState.leftPanelT = LerpF(gState.leftPanelT, gState.showLeftPanel ? 1.0f : 0.0f, t);
    gState.rightPanelT = LerpF(gState.rightPanelT, gState.showRightPanel ? 1.0f : 0.0f, t);
    gState.statusPanelT = LerpF(gState.statusPanelT, gState.statusExpanded ? 1.0f : 0.0f, t);

    bool textEditing = (gState.renameIndex >= 0) || (gState.tabRenameIndex >= 0);
    bool modalOpen = gState.showDeleteConfirm || gState.showProjectDialog || gState.showUnsavedConfirm || gState.showRecoveryDialog;
    gBlockUiInput = textEditing || modalOpen;
    if (!textEditing && !modalOpen) {
        bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrlDown && IsKeyPressed(KEY_C)) {
            CopySelectionToClipboard(gState);
        }
        if (ctrlDown && IsKeyPressed(KEY_V)) {
            PasteClipboard(gState);
        }
        if (ctrlDown && IsKeyPressed(KEY_X)) {
            CutSelection(gState);
        }
        if (ctrlDown && IsKeyPressed(KEY_D)) {
            DuplicateSelection(gState);
        }

        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
            UndoLastAction(gState);
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
            RedoLastAction(gState);
        }

        if (IsKeyPressed(KEY_W)) {
            gState.selectMode = false;
            gState.gizmoMode = GizmoMode::Move;
        }
        if (IsKeyPressed(KEY_E)) {
            gState.selectMode = false;
            gState.gizmoMode = GizmoMode::Rotate;
        }
        if (IsKeyPressed(KEY_R)) {
            gState.selectMode = false;
            gState.gizmoMode = GizmoMode::Scale;
        }

        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) &&
            gState.selectedIndex >= 0 && gState.selectedIndex < (int)gState.objects.size()) {
            if (gState.selectedIndices.size() > 1) RequestDeleteSelection(gState);
            else RequestDeleteObject(gState, gState.selectedIndex);
        }
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    const float topH = 42.0f;
    const float subTopH = 34.0f;
    const float iconBarBaseW = 68.0f;
    const float leftPanelBaseW = Clampf((float)sw * 0.24f, 220.0f, 320.0f);
    const float rightBaseW = Clampf((float)sw * 0.27f, 250.0f, 360.0f);
    float iconBarW = iconBarBaseW * gState.leftPanelT;
    float leftPanelW = leftPanelBaseW * gState.leftPanelT;
    float rightW = rightBaseW * gState.rightPanelT;
    const float statusMinH = 28.0f;
    const float statusMaxH = 182.0f;
    const float statusH = statusMinH + (statusMaxH - statusMinH) * gState.statusPanelT;
    const float bodyTop = topH + subTopH;
    const float statusY = (float)sh - statusH;

    float minViewportW = 320.0f;
    float totalSide = iconBarW + leftPanelW + rightW;
    float maxSide = (float)sw - minViewportW - 4.0f;
    if (totalSide > maxSide && maxSide > 0.0f) {
        float over = totalSide - maxSide;
        if (rightW > 0.0f) {
            float cut = fminf(over, rightW);
            rightW -= cut;
            over -= cut;
        }
        if (over > 0.0f && leftPanelW > 0.0f) {
            float cut = fminf(over, leftPanelW);
            leftPanelW -= cut;
            over -= cut;
        }
        if (over > 0.0f && iconBarW > 0.0f) {
            float cut = fminf(over, iconBarW);
            iconBarW -= cut;
        }
    }

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

    // updates orbit camera controls while interacting in the 3d viewport
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

    // updates selection and gizmo interactions inside the 3d viewport
    if (!gState.viewport2D && viewportInputEnabled) {
        if (gState.selectMode) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                !(IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
                const int picked = PickObject3D(gState, viewport);
                bool pickedInSelection = false;
                for (int idx : gState.selectedIndices) {
                    if (idx == picked) {
                        pickedInSelection = true;
                        break;
                    }
                }

                if (pickedInSelection && !gState.selectedIndices.empty()) {
                    gState.selectionDragActive = true;
                    gState.selectionDragStart = GetMousePosition();
                    gState.boxSelecting = false;
                } else {
                    gState.selectionDragActive = false;
                    gState.boxSelecting = true;
                    gState.boxSelectStart = GetMousePosition();
                    gState.boxSelectEnd = gState.boxSelectStart;
                }
            }

            if (gState.boxSelecting && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                gState.boxSelectEnd = GetMousePosition();
            }

            if (gState.boxSelecting && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                gState.boxSelecting = false;
                const float minX = fminf(gState.boxSelectStart.x, gState.boxSelectEnd.x);
                const float minY = fminf(gState.boxSelectStart.y, gState.boxSelectEnd.y);
                const float maxX = fmaxf(gState.boxSelectStart.x, gState.boxSelectEnd.x);
                const float maxY = fmaxf(gState.boxSelectStart.y, gState.boxSelectEnd.y);
                Rectangle selRect = { minX, minY, maxX - minX, maxY - minY };
                const bool clickSelect = selRect.width < 4.0f && selRect.height < 4.0f;
                if (clickSelect) {
                    int picked = PickObject3D(gState, viewport);
                    gState.selectedIndices.clear();
                    if (picked >= 0 && picked < (int)gState.objects.size()) {
                        gState.selectedIndices.push_back(picked);
                        gState.selectedIndex = picked;
                    } else {
                        gState.selectedIndex = -1;
                    }
                    gState.colorSyncIndex = -1;
                } else {
                    gState.selectedIndices.clear();
                    for (int i = 0; i < (int)gState.objects.size(); i++) {
                        const EditorObject &obj = gState.objects[i];
                        if (!obj.visible) continue;
                        if (!IsObjectLayerVisible(gState, obj)) continue;
                        if (gState.isolateSelected && gState.selectedIndex >= 0 && i != gState.selectedIndex) continue;
                        Vector2 screenPos = GetWorldToScreen(obj.position, gState.camera);
                        if (CheckCollisionPointRec(screenPos, selRect)) gState.selectedIndices.push_back(i);
                    }
                    if (!gState.selectedIndices.empty()) {
                        gState.selectedIndex = gState.selectedIndices[0];
                    } else {
                        gState.selectedIndex = -1;
                    }
                }
                gState.colorSyncIndex = -1;
            }
        } else {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                !(IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
                gState.leftDragNavigate = false;
                gState.activeAxis = GizmoAxis::None;
                gState.draggingObjectMove = false;
                bool handledByGizmo = false;

                if (gState.selectedIndex >= 0 && gState.selectedIndex < (int)gState.objects.size()) {
                    EditorObject &selectedObj = gState.objects[gState.selectedIndex];
                    if (!selectedObj.anchored && IsObjectLayerVisible(gState, selectedObj)) {
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
                    gState.selectedIndices.clear();

                    if (picked >= 0 && picked < (int)gState.objects.size()) {
                        gState.selectedIndices.push_back(picked);
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
                        Vector2 delta = GetMouseDelta();
                        if (gState.selectedIndices.size() > 1) {
                            for (int idx : gState.selectedIndices) {
                                if (idx < 0 || idx >= (int)gState.objects.size()) continue;
                                EditorObject &selObj = gState.objects[idx];
                                if (selObj.anchored) continue;
                                if (!IsObjectLayerVisible(gState, selObj)) continue;
                                ApplyMoveAxisDrag(gState, selObj, gState.activeAxis, delta);
                            }
                        } else {
                            ApplyMoveAxisDrag(gState, obj, gState.activeAxis, delta);
                        }
                    } else if (gState.draggingObjectMove) {
                        Ray ray = GetMouseRay(GetMousePosition(), gState.camera);
                        Vector3 hit = obj.position;
                        if (RayPlaneIntersection(ray, gState.moveDragPlanePoint, gState.moveDragPlaneNormal, &hit)) {
                            Vector3 prevPos = obj.position;
                            obj.position = Vector3Add(hit, gState.moveDragOffset);
                            if (gState.selectedIndices.size() > 1) {
                                Vector3 delta = Vector3Subtract(obj.position, prevPos);
                                for (int idx : gState.selectedIndices) {
                                    if (idx == gState.selectedIndex) continue;
                                    if (idx < 0 || idx >= (int)gState.objects.size()) continue;
                                    EditorObject &selObj = gState.objects[idx];
                                    if (selObj.anchored) continue;
                                    if (!IsObjectLayerVisible(gState, selObj)) continue;
                                    selObj.position = Vector3Add(selObj.position, delta);
                                }
                            }
                        }
                    }
                } else if (!obj.anchored && gState.activeAxis != GizmoAxis::None &&
                           (gState.gizmoMode == GizmoMode::Rotate || gState.gizmoMode == GizmoMode::Scale)) {
                    Vector2 delta = GetMouseDelta();
                    if (gState.selectedIndices.size() > 1) {
                        for (int idx : gState.selectedIndices) {
                            if (idx < 0 || idx >= (int)gState.objects.size()) continue;
                            EditorObject &selObj = gState.objects[idx];
                            if (selObj.anchored) continue;
                            if (!IsObjectLayerVisible(gState, selObj)) continue;
                            ApplyGizmoDrag(selObj, gState.gizmoMode, gState.activeAxis, delta,
                                           gState.gizmoRotateSensitivity, gState.gizmoScaleSensitivity);
                        }
                    } else {
                        ApplyGizmoDrag(obj, gState.gizmoMode, gState.activeAxis, delta,
                                       gState.gizmoRotateSensitivity, gState.gizmoScaleSensitivity);
                    }
                }
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                gState.leftDragNavigate = false;
                gState.draggingObjectMove = false;
                gState.activeAxis = GizmoAxis::None;
            }
        }
    } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        gState.leftDragNavigate = false;
        gState.draggingObjectMove = false;
        gState.activeAxis = GizmoAxis::None;
        gState.boxSelecting = false;
        if (!gState.selectMode) gState.selectionDragActive = false;
    }

    // delegates static ui placement and visuals to the layout module
    DrawMainGuiLayout(sw, sh, topH, subTopH, bodyTop, statusY, statusH, leftIcons, leftPanel, viewport, rightPanel, statusBar);

    if (gState.requestSwitchWorkspace) {
        gState.requestSwitchWorkspace = false;
        SwitchWorkspace(gState, gState.requestSwitchWorkspaceIndex);
        ResetHistoryBaseline(gState);
        gState.selectionDragActive = false;
    }

    if (gState.selectionDragActive && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (gState.hoveredWorkspaceTab >= 0) {
            CopySelectionToWorkspace(gState, gState.hoveredWorkspaceTab);
        }
        gState.selectionDragActive = false;
    }

    // updates undo/redo capture after all frame actions have been applied
    UpdateHistoryTracker(gState);
    UpdateProjectAutosave(gState);
}

// cleans up temporary files and resets editor globals on engine shutdown
void ShutdownEngineGuiLayout() {
    if (!gState.initialized) return;
    RecordCacheAction(gState, "session", "Engine editor session ended");
    ShutdownProjectPersistence(gState);
    RemoveSessionCacheFile(gState);
    gState = {};
    gHoverTooltip = nullptr;
    gBlockUiInput = false;
}
