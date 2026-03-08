#include "../include/vehicle_import.hpp"
#include "../include/gui_manager.hpp"
#include "../include/persistence.hpp"
#include "../include/persistence_picker.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// keep a clean import object name based on primitive label
static std::string BuildImportObjectName(PrimitiveKind kind, int ordinal) {
    const PrimitiveDef *def = FindPrimitiveDef(kind);
    const char *base = (def && def->label && def->label[0]) ? def->label : "Block";
    char out[64] = { 0 };
    std::snprintf(out, sizeof(out), "%s_%02d", base, ordinal);
    return out;
}

// parse one object line from .vehicle export format
static bool ParseVehicleObject(const std::string &line, EditorObject *outObj) {
    if (outObj == nullptr) return false;

    int kind = 0;
    int visible = 1;
    int is2D = 0;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    int cr = 255, cg = 255, cb = 255, ca = 255;

    int read = std::sscanf(line.c_str(),
                           "obj=%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%d,%d",
                           &kind, &visible, &is2D,
                           &px, &py, &pz,
                           &rx, &ry, &rz,
                           &sx, &sy, &sz,
                           &cr, &cg, &cb, &ca);
    if (read != 16) return false;
    if (kind < 0 || kind >= kPrimitiveDefCount) return false;

    EditorObject obj = {};
    obj.name = "Imported";
    obj.kind = (PrimitiveKind)kind;
    obj.is2D = (is2D != 0);
    obj.visible = (visible != 0);
    obj.anchored = false;
    obj.layerIndex = 0;
    obj.parentIndex = -1;
    obj.position = (Vector3){ px, py, pz };
    obj.rotation = (Vector3){ rx, ry, rz };
    obj.scale = (Vector3){ sx, sy, sz };
    obj.color = (Color){
        (unsigned char)std::clamp(cr, 0, 255),
        (unsigned char)std::clamp(cg, 0, 255),
        (unsigned char)std::clamp(cb, 0, 255),
        (unsigned char)std::clamp(ca, 0, 255)
    };

    *outObj = obj;
    return true;
}

// parse one shotpoint line from .vehicle export format
static bool ParseVehicleShotpoint(const std::string &line, Shotpoint *outSp) {
    if (outSp == nullptr) return false;

    int objectIndex = -1;
    int enabled = 1;
    float lx = 0.0f, ly = 0.0f, lz = 0.0f;
    int cr = 240, cg = 90, cb = 90, ca = 255;
    float blastSize = 0.16f;

    int read = std::sscanf(line.c_str(),
                           "sp=%d,%d,%f,%f,%f,%d,%d,%d,%d,%f",
                           &objectIndex, &enabled,
                           &lx, &ly, &lz,
                           &cr, &cg, &cb, &ca,
                           &blastSize);
    if (read != 10) return false;

    Shotpoint sp = {};
    char name[64] = { 0 };
    std::snprintf(name, sizeof(name), "SP_%02d", 1);
    sp.name = name;
    sp.objectIndex = objectIndex;
    sp.enabled = (enabled != 0);
    sp.localPos = (Vector3){ lx, ly, lz };
    sp.blastColor = (Color){
        (unsigned char)std::clamp(cr, 0, 255),
        (unsigned char)std::clamp(cg, 0, 255),
        (unsigned char)std::clamp(cb, 0, 255),
        (unsigned char)std::clamp(ca, 0, 255)
    };
    sp.blastSize = Clampf(blastSize, 0.05f, 2.0f);

    *outSp = sp;
    return true;
}

// import one .vehicle file and replace current scene objects shotpoints and direction
bool ImportVehicleFile(EditorGuiState &st, const std::string &filePath) {
    if (filePath.empty()) {
        AddLog(st, "import failed empty file path");
        return false;
    }

    std::ifstream in(filePath, std::ios::in);
    if (!in.is_open()) {
        AddLog(st, "import failed could not open .vehicle file");
        return false;
    }

    std::string line;
    bool headerOk = false;
    std::string importedName;
    float importedYaw = 0.0f;
    std::vector<EditorObject> importedObjects;
    std::vector<Shotpoint> importedShotpoints;

    while (std::getline(in, line)) {
        if (line.rfind("LBFE_VEHICLE_V1", 0) == 0) {
            headerOk = true;
            continue;
        }
        if (line.rfind("vehicle_name=", 0) == 0) {
            importedName = line.substr(13);
            continue;
        }
        if (line.rfind("forward_yaw_deg=", 0) == 0) {
            float yaw = 0.0f;
            if (std::sscanf(line.c_str() + 16, "%f", &yaw) == 1) importedYaw = Clampf(yaw, -180.0f, 180.0f);
            continue;
        }
        if (line.rfind("obj=", 0) == 0) {
            EditorObject obj = {};
            if (ParseVehicleObject(line, &obj)) importedObjects.push_back(obj);
            continue;
        }
        if (line.rfind("sp=", 0) == 0) {
            Shotpoint sp = {};
            if (ParseVehicleShotpoint(line, &sp)) importedShotpoints.push_back(sp);
            continue;
        }
        if (line.rfind("end", 0) == 0) break;
    }

    if (!headerOk) {
        AddLog(st, "import failed invalid vehicle header");
        return false;
    }
    if (importedObjects.empty()) {
        AddLog(st, "import failed vehicle has no objects");
        return false;
    }

    std::unordered_map<std::string, int> names;
    for (int i = 0; i < (int)importedObjects.size(); i++) {
        EditorObject &obj = importedObjects[i];
        obj.layerIndex = 0;
        obj.parentIndex = -1;
        std::string candidate = BuildImportObjectName(obj.kind, i + 1);
        auto it = names.find(candidate);
        if (it != names.end()) {
            it->second++;
            char suffix[16] = { 0 };
            std::snprintf(suffix, sizeof(suffix), "_%d", it->second);
            candidate += suffix;
        } else {
            names[candidate] = 1;
        }
        obj.name = candidate;
    }

    for (int i = 0; i < (int)importedShotpoints.size(); i++) {
        Shotpoint &sp = importedShotpoints[i];
        char name[64] = { 0 };
        std::snprintf(name, sizeof(name), "SP_%02d", i + 1);
        sp.name = name;
        if (sp.objectIndex < 0 || sp.objectIndex >= (int)importedObjects.size()) {
            sp.objectIndex = -1;
            sp.enabled = false;
        }
    }

    st.objects = importedObjects;
    st.shotpoints = importedShotpoints;
    st.layers.clear();
    EditorLayer def = {};
    def.name = "Default";
    def.visible = true;
    st.layers.push_back(def);
    st.activeLayerIndex = 0;

    st.selectedIndices.clear();
    st.selectedIndex = st.objects.empty() ? -1 : 0;
    if (st.selectedIndex >= 0) st.selectedIndices.push_back(st.selectedIndex);
    st.selectedShotpoint = st.shotpoints.empty() ? -1 : 0;
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;
    st.vehicleForwardYawDeg = importedYaw;

    if (!importedName.empty()) {
        if (!st.projectLoaded || st.currentProjectName.empty() || st.currentProjectName == "no project") {
            st.currentProjectName = importedName;
        }
    }

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
    }
    MarkProjectDirty(st);
    ResetHistoryBaseline(st);

    char msg[256] = { 0 };
    std::snprintf(msg, sizeof(msg),
                  "imported vehicle %s (%d objects, %d shotpoints)",
                  importedName.empty() ? "unnamed" : importedName.c_str(),
                  (int)st.objects.size(), (int)st.shotpoints.size());
    AddLog(st, msg);
    RecordCacheAction(st, "vehicle_import", filePath.c_str());
    return true;
}

// opens a native file picker and imports one selected .vehicle file
bool ImportVehicleFromPicker(EditorGuiState &st) {
    std::string path = PickFilePathNative("import vehicle", "*.vehicle");
    if (path.empty()) {
        AddLog(st, "import cancelled");
        return false;
    }
    return ImportVehicleFile(st, path);
}
