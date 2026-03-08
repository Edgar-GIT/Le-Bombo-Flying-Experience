#include "../include/piece_library.hpp"
#include "../include/gui_manager.hpp"
#include "../include/persistence.hpp"
#include "../include/workspace_tabs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// resolves project root by finding main/src/main.c marker
static std::filesystem::path ResolveProjectRootPath() {
    std::filesystem::path current = std::filesystem::current_path();
    while (true) {
        if (std::filesystem::exists(current / "main" / "src" / "main.c")) return current;
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return std::filesystem::current_path();
}

// removes unsupported chars from names used in file paths
static std::string SanitizeName(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u) || c == '_' || c == '-') out.push_back(c);
        else if (c == ' ') out.push_back('_');
    }
    if (out.empty()) out = "piece";
    return out;
}

// trims spaces from both sides of one string
static std::string Trim(const std::string &raw) {
    size_t a = 0;
    while (a < raw.size() && std::isspace((unsigned char)raw[a])) a++;
    size_t b = raw.size();
    while (b > a && std::isspace((unsigned char)raw[b - 1])) b--;
    return raw.substr(a, b - a);
}

// serializes one piece template to .piece file
static bool SavePieceTemplateFile(const std::string &filePath, const PieceTemplate &piece) {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;

    out << "LBFE_PIECE_V1\n";
    out << "piece_name=" << SanitizeName(piece.name) << "\n";

    for (size_t i = 0; i < piece.snapshot.layerNames.size(); i++) {
        const bool vis = i < piece.snapshot.layerVisible.size() ? piece.snapshot.layerVisible[i] : true;
        out << "layer=" << SanitizeName(piece.snapshot.layerNames[i]) << "," << (vis ? 1 : 0) << "\n";
    }

    for (const EditorObject &obj : piece.snapshot.objects) {
        out << "obj="
            << (int)obj.kind << ","
            << (obj.visible ? 1 : 0) << ","
            << (obj.is2D ? 1 : 0) << ","
            << obj.layerIndex << ","
            << obj.parentIndex << ","
            << obj.position.x << "," << obj.position.y << "," << obj.position.z << ","
            << obj.rotation.x << "," << obj.rotation.y << "," << obj.rotation.z << ","
            << obj.scale.x << "," << obj.scale.y << "," << obj.scale.z << ","
            << (int)obj.color.r << "," << (int)obj.color.g << "," << (int)obj.color.b << "," << (int)obj.color.a
            << "\n";
    }

    for (const Shotpoint &sp : piece.snapshot.shotpoints) {
        out << "sp="
            << SanitizeName(sp.name) << ","
            << sp.objectIndex << ","
            << (sp.enabled ? 1 : 0) << ","
            << sp.localPos.x << "," << sp.localPos.y << "," << sp.localPos.z << ","
            << (int)sp.blastColor.r << "," << (int)sp.blastColor.g << "," << (int)sp.blastColor.b << "," << (int)sp.blastColor.a << ","
            << sp.blastSize
            << "\n";
    }

    out << "end\n";
    return true;
}

// parses one obj line from .piece file
static bool ParseObjLine(const std::string &line, EditorObject *outObj) {
    int kind = 0;
    int visible = 1;
    int is2D = 0;
    int layer = 0;
    int parent = -1;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    int cr = 255, cg = 255, cb = 255, ca = 255;

    int read = std::sscanf(line.c_str(),
                           "obj=%d,%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%d,%d",
                           &kind, &visible, &is2D, &layer, &parent,
                           &px, &py, &pz,
                           &rx, &ry, &rz,
                           &sx, &sy, &sz,
                           &cr, &cg, &cb, &ca);
    if (read != 18) return false;

    EditorObject obj = {};
    obj.name = "piece_obj";
    obj.kind = (PrimitiveKind)kind;
    obj.visible = visible != 0;
    obj.is2D = is2D != 0;
    obj.anchored = false;
    obj.layerIndex = layer;
    obj.parentIndex = parent;
    obj.position = { px, py, pz };
    obj.rotation = { rx, ry, rz };
    obj.scale = { sx, sy, sz };
    obj.color = (Color){
        (unsigned char)std::clamp(cr, 0, 255),
        (unsigned char)std::clamp(cg, 0, 255),
        (unsigned char)std::clamp(cb, 0, 255),
        (unsigned char)std::clamp(ca, 0, 255),
    };
    *outObj = obj;
    return true;
}

// parses one shotpoint line from .piece file
static bool ParseShotpointLine(const std::string &line, Shotpoint *outSp) {
    char name[128] = { 0 };
    int objIndex = -1;
    int enabled = 1;
    float lx = 0.0f, ly = 0.0f, lz = 0.0f;
    int cr = 255, cg = 90, cb = 90, ca = 255;
    float size = 0.16f;

    int read = std::sscanf(line.c_str(),
                           "sp=%127[^,],%d,%d,%f,%f,%f,%d,%d,%d,%d,%f",
                           name, &objIndex, &enabled,
                           &lx, &ly, &lz,
                           &cr, &cg, &cb, &ca,
                           &size);
    if (read != 11) return false;

    Shotpoint sp = {};
    sp.name = name;
    sp.objectIndex = objIndex;
    sp.enabled = enabled != 0;
    sp.localPos = { lx, ly, lz };
    sp.blastColor = (Color){
        (unsigned char)std::clamp(cr, 0, 255),
        (unsigned char)std::clamp(cg, 0, 255),
        (unsigned char)std::clamp(cb, 0, 255),
        (unsigned char)std::clamp(ca, 0, 255),
    };
    sp.blastSize = std::clamp(size, 0.05f, 2.5f);
    *outSp = sp;
    return true;
}

// parses one layer line from .piece file
static bool ParseLayerLine(const std::string &line, std::string *outName, bool *outVisible) {
    char name[128] = { 0 };
    int visible = 1;
    int read = std::sscanf(line.c_str(), "layer=%127[^,],%d", name, &visible);
    if (read != 2) return false;
    *outName = Trim(name);
    *outVisible = visible != 0;
    return true;
}

// loads one .piece file into memory template
static bool LoadPieceTemplateFile(const std::string &filePath, PieceTemplate *outPiece) {
    std::ifstream in(filePath, std::ios::in);
    if (!in.is_open()) return false;

    PieceTemplate piece = {};
    piece.filePath = filePath;
    piece.name = "piece";

    bool headerOk = false;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty()) continue;
        if (line == "LBFE_PIECE_V1") {
            headerOk = true;
            continue;
        }
        if (line.rfind("piece_name=", 0) == 0) {
            piece.name = SanitizeName(line.substr(11));
            continue;
        }
        if (line.rfind("layer=", 0) == 0) {
            std::string name;
            bool vis = true;
            if (ParseLayerLine(line, &name, &vis)) {
                piece.snapshot.layerNames.push_back(name);
                piece.snapshot.layerVisible.push_back(vis);
            }
            continue;
        }
        if (line.rfind("obj=", 0) == 0) {
            EditorObject obj = {};
            if (ParseObjLine(line, &obj)) {
                char objName[64] = { 0 };
                std::snprintf(objName, sizeof(objName), "%s_obj_%02d", piece.name.c_str(), (int)piece.snapshot.objects.size() + 1);
                obj.name = objName;
                piece.snapshot.objects.push_back(obj);
            }
            continue;
        }
        if (line.rfind("sp=", 0) == 0) {
            Shotpoint sp = {};
            if (ParseShotpointLine(line, &sp)) {
                piece.snapshot.shotpoints.push_back(sp);
            }
            continue;
        }
        if (line == "end") break;
    }

    if (!headerOk || piece.snapshot.objects.empty()) return false;
    if (piece.snapshot.layerNames.empty()) {
        piece.snapshot.layerNames.push_back("Default");
        piece.snapshot.layerVisible.push_back(true);
    }
    *outPiece = piece;
    return true;
}

// rebuilds the in-memory list by scanning pieces directory
void ReloadPieceLibrary(EditorGuiState &st) {
    st.pieceLibrary.clear();
    if (st.piecesRootPath.empty()) return;

    std::error_code ec;
    if (!std::filesystem::exists(st.piecesRootPath, ec)) return;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(st.piecesRootPath, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".piece") continue;
        PieceTemplate piece = {};
        if (LoadPieceTemplateFile(entry.path().string(), &piece)) {
            st.pieceLibrary.push_back(piece);
        }
    }

    std::sort(st.pieceLibrary.begin(), st.pieceLibrary.end(), [](const PieceTemplate &a, const PieceTemplate &b) {
        return a.name < b.name;
    });
}

// initializes piece folder and loads templates
void InitPieceLibrary(EditorGuiState &st) {
    const std::filesystem::path root = ResolveProjectRootPath();
    const std::filesystem::path pieces = root / "main" / "GameEngine" / "pieces";
    std::error_code ec;
    std::filesystem::create_directories(pieces, ec);
    st.piecesRootPath = pieces.string();
    ReloadPieceLibrary(st);
}

// creates piece from selected objects and opens a piece tab
bool CreatePieceFromSelection(EditorGuiState &st) {
    std::vector<int> indices = st.selectedIndices;
    if (indices.empty() && st.selectedIndex >= 0 && st.selectedIndex < (int)st.objects.size()) {
        indices.push_back(st.selectedIndex);
    }
    if (indices.empty()) {
        AddLog(st, "select blocks before creating a piece");
        return false;
    }

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    EditorSnapshot snap = {};
    std::unordered_map<int, int> remap;
    Vector3 center = { 0.0f, 0.0f, 0.0f };

    for (int idx : indices) {
        if (idx < 0 || idx >= (int)st.objects.size()) continue;
        remap[idx] = (int)snap.objects.size();
        EditorObject obj = st.objects[idx];
        obj.parentIndex = -1;
        snap.objects.push_back(obj);
        center.x += obj.position.x;
        center.y += obj.position.y;
        center.z += obj.position.z;
    }
    if (snap.objects.empty()) {
        AddLog(st, "could not create piece from current selection");
        return false;
    }

    const float inv = 1.0f / (float)snap.objects.size();
    center.x *= inv;
    center.y *= inv;
    center.z *= inv;
    for (EditorObject &obj : snap.objects) {
        obj.position.x -= center.x;
        obj.position.y -= center.y;
        obj.position.z -= center.z;
    }

    for (const Shotpoint &sp : st.shotpoints) {
        auto it = remap.find(sp.objectIndex);
        if (it == remap.end()) continue;
        Shotpoint copy = sp;
        copy.objectIndex = it->second;
        snap.shotpoints.push_back(copy);
    }

    for (const EditorLayer &layer : st.layers) {
        snap.layerNames.push_back(layer.name);
        snap.layerVisible.push_back(layer.visible);
    }
    if (snap.layerNames.empty()) {
        snap.layerNames.push_back("Default");
        snap.layerVisible.push_back(true);
    }

    int next = 1;
    while (true) {
        char candidate[64] = { 0 };
        std::snprintf(candidate, sizeof(candidate), "piece_%02d", next);
        bool used = false;
        for (const PieceTemplate &p : st.pieceLibrary) {
            if (p.name == candidate) {
                used = true;
                break;
            }
        }
        if (!used) {
            PieceTemplate piece = {};
            piece.name = candidate;
            piece.snapshot = snap;
            piece.filePath = (std::filesystem::path(st.piecesRootPath) / (piece.name + ".piece")).string();
            if (!SavePieceTemplateFile(piece.filePath, piece)) {
                AddLog(st, "failed to write piece file");
                return false;
            }
            st.pieceLibrary.push_back(piece);
            int tab = FindWorkspaceByPath(st, piece.filePath);
            if (tab < 0) tab = AddWorkspace(st, WorkspaceKind::Piece, piece.name, piece.filePath, piece.snapshot);
            SwitchWorkspace(st, tab);
            AddLog(st, "piece created");
            RecordCacheAction(st, "piece_create", piece.name.c_str());
            return true;
        }
        next++;
    }
}

// saves currently active piece workspace to .piece file
bool SaveActivePieceWorkspace(EditorGuiState &st) {
    if (st.activeWorkspaceIndex < 0 || st.activeWorkspaceIndex >= (int)st.workspaces.size()) return false;
    WorkspaceTab &tab = st.workspaces[st.activeWorkspaceIndex];
    if (tab.kind != WorkspaceKind::Piece) return false;

    SyncActiveWorkspace(st);
    PieceTemplate piece = {};
    piece.name = tab.name;
    piece.filePath = tab.filePath;
    piece.snapshot = tab.snapshot;
    if (piece.filePath.empty()) {
        piece.filePath = (std::filesystem::path(st.piecesRootPath) / (SanitizeName(piece.name) + ".piece")).string();
        tab.filePath = piece.filePath;
    }
    if (!SavePieceTemplateFile(piece.filePath, piece)) return false;

    bool found = false;
    for (PieceTemplate &p : st.pieceLibrary) {
        if (p.filePath == piece.filePath) {
            p = piece;
            found = true;
            break;
        }
    }
    if (!found) st.pieceLibrary.push_back(piece);
    tab.dirty = false;
    AddLog(st, "piece saved");
    RecordCacheAction(st, "piece_save", piece.name.c_str());
    return true;
}

// spawns a piece template into current workspace
bool SpawnPieceTemplateAt(EditorGuiState &st, int pieceIndex, Vector3 spawnPos, bool selectPlaced) {
    if (pieceIndex < 0 || pieceIndex >= (int)st.pieceLibrary.size()) return false;
    const PieceTemplate &piece = st.pieceLibrary[pieceIndex];
    if (piece.snapshot.objects.empty()) return false;

    std::unordered_map<std::string, int> layerRemap;
    for (int i = 0; i < (int)piece.snapshot.layerNames.size(); i++) {
        const std::string &layerName = piece.snapshot.layerNames[i];
        int found = -1;
        for (int j = 0; j < (int)st.layers.size(); j++) {
            if (st.layers[j].name == layerName) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            EditorLayer layer = {};
            layer.name = layerName;
            layer.visible = i < (int)piece.snapshot.layerVisible.size() ? piece.snapshot.layerVisible[i] : true;
            st.layers.push_back(layer);
            found = (int)st.layers.size() - 1;
        }
        layerRemap[layerName] = found;
    }

    const int base = (int)st.objects.size();
    st.selectedIndices.clear();
    for (int i = 0; i < (int)piece.snapshot.objects.size(); i++) {
        EditorObject obj = piece.snapshot.objects[i];
        obj.position.x += spawnPos.x;
        obj.position.y += spawnPos.y;
        obj.position.z += spawnPos.z;
        obj.parentIndex = obj.parentIndex >= 0 ? (base + obj.parentIndex) : -1;
        if (obj.layerIndex < 0 || obj.layerIndex >= (int)piece.snapshot.layerNames.size()) {
            obj.layerIndex = 0;
        } else {
            const std::string &name = piece.snapshot.layerNames[obj.layerIndex];
            auto it = layerRemap.find(name);
            obj.layerIndex = it == layerRemap.end() ? 0 : it->second;
        }
        char newName[64] = { 0 };
        std::snprintf(newName, sizeof(newName), "%s_%02d", piece.name.c_str(), i + 1);
        obj.name = newName;
        st.objects.push_back(obj);
        st.selectedIndices.push_back(base + i);
    }

    for (const Shotpoint &src : piece.snapshot.shotpoints) {
        Shotpoint sp = src;
        sp.objectIndex = (sp.objectIndex >= 0 && sp.objectIndex < (int)piece.snapshot.objects.size()) ? base + sp.objectIndex : -1;
        st.shotpoints.push_back(sp);
    }

    if (selectPlaced && !st.selectedIndices.empty()) st.selectedIndex = st.selectedIndices[0];
    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size()) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
        if (st.workspaces[st.activeWorkspaceIndex].kind == WorkspaceKind::Project) MarkProjectDirty(st);
    } else {
        MarkProjectDirty(st);
    }
    AddLog(st, "piece inserted into scene");
    RecordCacheAction(st, "piece_spawn", piece.name.c_str());
    return true;
}
