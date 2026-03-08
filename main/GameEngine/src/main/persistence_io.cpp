#include "../include/persistence_io.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// limita um inteiro a um intervalo
static int ClampI(int v, int minV, int maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

// limita um float a um intervalo
static float ClampF(float v, float minV, float maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

// escapa texto para json
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

// converte string json para string normal
static std::string UnescapeJson(const std::string &text) {
    std::string out;
    out.reserve(text.size());

    bool esc = false;
    for (char c : text) {
        if (esc) {
            if (c == 'n') out.push_back('\n');
            else if (c == 'r') out.push_back('\r');
            else if (c == 't') out.push_back('\t');
            else out.push_back(c);
            esc = false;
            continue;
        }

        if (c == '\\') {
            esc = true;
            continue;
        }

        out.push_back(c);
    }

    return out;
}

// le todo o ficheiro para string
static bool ReadTextFile(const std::filesystem::path &path, std::string *outText) {
    std::ifstream in(path, std::ios::in);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    *outText = ss.str();
    return true;
}

// procura um valor string por chave
static bool ReadStringField(const std::string &src, const char *key, std::string *outValue) {
    std::string token = std::string("\"") + key + "\":\"";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    std::string raw;
    bool esc = false;
    for (size_t i = start; i < src.size(); i++) {
        char c = src[i];
        if (esc) {
            raw.push_back(c);
            esc = false;
            continue;
        }
        if (c == '\\') {
            esc = true;
            raw.push_back(c);
            continue;
        }
        if (c == '"') {
            *outValue = UnescapeJson(raw);
            return true;
        }
        raw.push_back(c);
    }

    return false;
}

// procura um valor booleano por chave
static bool ReadBoolField(const std::string &src, const char *key, bool *outValue) {
    std::string token = std::string("\"") + key + "\":";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    if (src.compare(start, 4, "true") == 0) {
        *outValue = true;
        return true;
    }
    if (src.compare(start, 5, "false") == 0) {
        *outValue = false;
        return true;
    }

    return false;
}

// procura um valor inteiro por chave
static bool ReadIntField(const std::string &src, const char *key, int *outValue) {
    std::string token = std::string("\"") + key + "\":";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    int value = 0;
    if (std::sscanf(src.c_str() + start, "%d", &value) == 1) {
        *outValue = value;
        return true;
    }

    return false;
}

// procura um valor float por chave
static bool ReadFloatField(const std::string &src, const char *key, float *outValue) {
    std::string token = std::string("\"") + key + "\":";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    float value = 0.0f;
    if (std::sscanf(src.c_str() + start, "%f", &value) == 1) {
        *outValue = value;
        return true;
    }

    return false;
}

// procura um vetor de 3 floats por chave
static bool ReadVec3Field(const std::string &src, const char *key, Vector3 *outValue) {
    std::string token = std::string("\"") + key + "\":[";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (std::sscanf(src.c_str() + start, "%f,%f,%f", &x, &y, &z) == 3) {
        outValue->x = x;
        outValue->y = y;
        outValue->z = z;
        return true;
    }

    return false;
}

// procura uma cor rgba por chave
static bool ReadColorField(const std::string &src, const char *key, Color *outColor) {
    std::string token = std::string("\"") + key + "\":[";
    size_t start = src.find(token);
    if (start == std::string::npos) return false;
    start += token.size();

    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
    if (std::sscanf(src.c_str() + start, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
        outColor->r = (unsigned char)ClampI(r, 0, 255);
        outColor->g = (unsigned char)ClampI(g, 0, 255);
        outColor->b = (unsigned char)ClampI(b, 0, 255);
        outColor->a = (unsigned char)ClampI(a, 0, 255);
        return true;
    }

    return false;
}

// escreve um objeto da cena em json
static void WriteObjectJson(std::ofstream &out, const EditorObject &obj, bool addComma) {
    out << "    {";
    out << "\"name\":\"" << EscapeJson(obj.name) << "\",";
    out << "\"kind\":" << (int)obj.kind << ",";
    out << "\"is2d\":" << (obj.is2D ? "true" : "false") << ",";
    out << "\"visible\":" << (obj.visible ? "true" : "false") << ",";
    out << "\"anchored\":" << (obj.anchored ? "true" : "false") << ",";
    out << "\"layer_index\":" << obj.layerIndex << ",";
    out << "\"parent_index\":" << obj.parentIndex << ",";
    out << "\"position\":[" << obj.position.x << "," << obj.position.y << "," << obj.position.z << "],";
    out << "\"rotation\":[" << obj.rotation.x << "," << obj.rotation.y << "," << obj.rotation.z << "],";
    out << "\"scale\":[" << obj.scale.x << "," << obj.scale.y << "," << obj.scale.z << "],";
    out << "\"color\":[" << (int)obj.color.r << "," << (int)obj.color.g << "," << (int)obj.color.b << "," << (int)obj.color.a << "]";
    out << "}";
    if (addComma) out << ",";
    out << "\n";
}

// escreve uma layer em json
static void WriteLayerJson(std::ofstream &out, const EditorLayer &layer, bool addComma) {
    out << "    {";
    out << "\"name\":\"" << EscapeJson(layer.name) << "\",";
    out << "\"visible\":" << (layer.visible ? "true" : "false");
    out << "}";
    if (addComma) out << ",";
    out << "\n";
}

// escreve um shotpoint em json
static void WriteShotpointJson(std::ofstream &out, const Shotpoint &sp, bool addComma) {
    out << "    {";
    out << "\"name\":\"" << EscapeJson(sp.name) << "\",";
    out << "\"object_index\":" << sp.objectIndex << ",";
    out << "\"enabled\":" << (sp.enabled ? "true" : "false") << ",";
    out << "\"local_pos\":[" << sp.localPos.x << "," << sp.localPos.y << "," << sp.localPos.z << "],";
    out << "\"blast_color\":[" << (int)sp.blastColor.r << "," << (int)sp.blastColor.g << "," << (int)sp.blastColor.b << "," << (int)sp.blastColor.a << "],";
    out << "\"blast_size\":" << sp.blastSize;
    out << "}";
    if (addComma) out << ",";
    out << "\n";
}

// grava json completo de projeto e estado do editor
bool SaveProjectJsonFile(const std::filesystem::path &filePath, const EditorGuiState &st) {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"project_name\":\"" << EscapeJson(st.currentProjectName) << "\",\n";

    out << "  \"ui\": {\n";
    out << "    \"viewport2d\":" << (st.viewport2D ? "true" : "false") << ",\n";
    out << "    \"show_left_panel\":" << (st.showLeftPanel ? "true" : "false") << ",\n";
    out << "    \"show_right_panel\":" << (st.showRightPanel ? "true" : "false") << ",\n";
    out << "    \"status_expanded\":" << (st.statusExpanded ? "true" : "false") << ",\n";
    out << "    \"isolate_selected\":" << (st.isolateSelected ? "true" : "false") << ",\n";
    out << "    \"left_tab\":" << (int)st.leftTab << ",\n";
    out << "    \"filter_mode\":" << (int)st.filterMode << ",\n";
    out << "    \"gizmo_mode\":" << (int)st.gizmoMode << "\n";
    out << "  },\n";

    out << "  \"camera\": {\n";
    out << "    \"orbit_target\":[" << st.orbitTarget.x << "," << st.orbitTarget.y << "," << st.orbitTarget.z << "],\n";
    out << "    \"orbit_yaw\":" << st.orbitYaw << ",\n";
    out << "    \"orbit_pitch\":" << st.orbitPitch << ",\n";
    out << "    \"orbit_distance\":" << st.orbitDistance << "\n";
    out << "  },\n";

    out << "  \"settings\": {\n";
    out << "    \"camera_orbit_sensitivity\":" << st.cameraOrbitSensitivity << ",\n";
    out << "    \"camera_pan_sensitivity\":" << st.cameraPanSensitivity << ",\n";
    out << "    \"camera_zoom_sensitivity\":" << st.cameraZoomSensitivity << ",\n";
    out << "    \"gizmo_move_sensitivity\":" << st.gizmoMoveSensitivity << ",\n";
    out << "    \"gizmo_rotate_sensitivity\":" << st.gizmoRotateSensitivity << ",\n";
    out << "    \"gizmo_scale_sensitivity\":" << st.gizmoScaleSensitivity << ",\n";
    out << "    \"preview_move_speed\":" << st.previewMoveSpeed << ",\n";
    out << "    \"preview_look_sensitivity\":" << st.previewLookSensitivity << ",\n";
    out << "    \"preview_zoom_speed\":" << st.previewZoomSpeed << ",\n";
    out << "    \"preview_pan_sensitivity\":" << st.previewPanSensitivity << ",\n";
    out << "    \"vehicle_forward_yaw\":" << st.vehicleForwardYawDeg << "\n";
    out << "  },\n";

    out << "  \"layers\": [\n";
    for (size_t i = 0; i < st.layers.size(); i++) {
        WriteLayerJson(out, st.layers[i], i + 1 < st.layers.size());
    }
    out << "  ],\n";

    out << "  \"objects\": [\n";
    for (size_t i = 0; i < st.objects.size(); i++) {
        WriteObjectJson(out, st.objects[i], i + 1 < st.objects.size());
    }
    out << "  ],\n";

    out << "  \"shotpoints\": [\n";
    for (size_t i = 0; i < st.shotpoints.size(); i++) {
        WriteShotpointJson(out, st.shotpoints[i], i + 1 < st.shotpoints.size());
    }
    out << "  ]\n";

    out << "}\n";
    return true;
}

// converte um bloco json para objeto da cena
static bool ParseObjectBlock(const std::string &block, EditorObject *outObj) {
    EditorObject obj = {};
    obj.name = "block";
    obj.kind = PrimitiveKind::Cube;
    obj.is2D = false;
    obj.visible = true;
    obj.anchored = false;
    obj.layerIndex = 0;
    obj.parentIndex = -1;
    obj.position = { 0.0f, 0.0f, 0.0f };
    obj.rotation = { 0.0f, 0.0f, 0.0f };
    obj.scale = { 1.0f, 1.0f, 1.0f };
    obj.color = WHITE;

    int kindValue = 0;
    ReadStringField(block, "name", &obj.name);
    if (ReadIntField(block, "kind", &kindValue)) obj.kind = (PrimitiveKind)kindValue;
    ReadBoolField(block, "is2d", &obj.is2D);
    ReadBoolField(block, "visible", &obj.visible);
    ReadBoolField(block, "anchored", &obj.anchored);
    ReadIntField(block, "layer_index", &obj.layerIndex);
    ReadIntField(block, "parent_index", &obj.parentIndex);
    ReadVec3Field(block, "position", &obj.position);
    ReadVec3Field(block, "rotation", &obj.rotation);
    ReadVec3Field(block, "scale", &obj.scale);
    ReadColorField(block, "color", &obj.color);

    *outObj = obj;
    return true;
}

// converte um bloco json para layer
static bool ParseLayerBlock(const std::string &block, EditorLayer *outLayer) {
    EditorLayer layer = {};
    layer.name = "Default";
    layer.visible = true;

    ReadStringField(block, "name", &layer.name);
    ReadBoolField(block, "visible", &layer.visible);
    if (layer.name.empty()) layer.name = "Layer";

    *outLayer = layer;
    return true;
}

// converte um bloco json para shotpoint
static bool ParseShotpointBlock(const std::string &block, Shotpoint *outSp) {
    Shotpoint sp = {};
    sp.name = "shotpoint";
    sp.objectIndex = -1;
    sp.enabled = true;
    sp.localPos = { 0.0f, 0.0f, 0.0f };
    sp.blastColor = (Color){ 240, 90, 90, 255 };
    sp.blastSize = 0.16f;

    ReadStringField(block, "name", &sp.name);
    ReadIntField(block, "object_index", &sp.objectIndex);
    ReadBoolField(block, "enabled", &sp.enabled);
    ReadVec3Field(block, "local_pos", &sp.localPos);
    ReadColorField(block, "blast_color", &sp.blastColor);
    ReadFloatField(block, "blast_size", &sp.blastSize);
    sp.blastSize = ClampF(sp.blastSize, 0.05f, 3.0f);

    *outSp = sp;
    return true;
}

// carrega o array de objetos do json
static bool ParseArrayBlocks(const std::string &json, const char *arrayKey, std::vector<std::string> *outBlocks) {
    outBlocks->clear();
    std::string keyToken = std::string("\"") + arrayKey + "\"";
    size_t keyPos = json.find(keyToken);
    if (keyPos == std::string::npos) return false;

    size_t arrStart = json.find('[', keyPos);
    if (arrStart == std::string::npos) return false;

    int depth = 0;
    size_t arrEnd = std::string::npos;
    for (size_t i = arrStart; i < json.size(); i++) {
        char c = json[i];
        if (c == '[') depth++;
        else if (c == ']') {
            depth--;
            if (depth == 0) {
                arrEnd = i;
                break;
            }
        }
    }
    if (arrEnd == std::string::npos) return false;

    size_t i = arrStart + 1;
    while (i < arrEnd) {
        while (i < arrEnd && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t' || json[i] == ',')) i++;
        if (i >= arrEnd) break;
        if (json[i] != '{') {
            i++;
            continue;
        }

        size_t blockStart = i;
        int objDepth = 0;
        bool foundEnd = false;
        for (; i < arrEnd; i++) {
            if (json[i] == '{') objDepth++;
            else if (json[i] == '}') {
                objDepth--;
                if (objDepth == 0) {
                    foundEnd = true;
                    break;
                }
            }
        }
        if (!foundEnd) break;

        size_t blockEnd = i;
        outBlocks->push_back(json.substr(blockStart, blockEnd - blockStart + 1));
        i = blockEnd + 1;
    }

    return true;
}

// carrega o array de objetos do json
static bool ParseObjectsArray(const std::string &json, std::vector<EditorObject> *outObjects) {
    outObjects->clear();
    std::vector<std::string> blocks;
    if (!ParseArrayBlocks(json, "objects", &blocks)) return false;
    for (const std::string &block : blocks) {
        EditorObject obj = {};
        if (ParseObjectBlock(block, &obj)) outObjects->push_back(obj);
    }
    return true;
}

// carrega o array de shotpoints do json
static bool ParseShotpointsArray(const std::string &json, std::vector<Shotpoint> *outShotpoints) {
    outShotpoints->clear();
    std::vector<std::string> blocks;
    if (!ParseArrayBlocks(json, "shotpoints", &blocks)) return true;
    for (const std::string &block : blocks) {
        Shotpoint sp = {};
        if (ParseShotpointBlock(block, &sp)) outShotpoints->push_back(sp);
    }
    return true;
}

// carrega o array de layers do json
static bool ParseLayersArray(const std::string &json, std::vector<EditorLayer> *outLayers) {
    outLayers->clear();
    std::vector<std::string> blocks;
    if (!ParseArrayBlocks(json, "layers", &blocks)) return true;
    for (const std::string &block : blocks) {
        EditorLayer layer = {};
        if (ParseLayerBlock(block, &layer)) outLayers->push_back(layer);
    }
    return true;
}

// carrega json de projeto e aplica no estado do editor
bool LoadProjectJsonFile(const std::filesystem::path &filePath, EditorGuiState *st) {
    if (!st) return false;

    std::string json;
    if (!ReadTextFile(filePath, &json)) return false;

    std::vector<EditorObject> loadedObjects;
    std::vector<Shotpoint> loadedShotpoints;
    std::vector<EditorLayer> loadedLayers;
    if (!ParseObjectsArray(json, &loadedObjects)) return false;
    if (!ParseShotpointsArray(json, &loadedShotpoints)) return false;
    if (!ParseLayersArray(json, &loadedLayers)) return false;

    st->objects = loadedObjects;
    st->shotpoints = loadedShotpoints;
    st->layers = loadedLayers;
    if (st->layers.empty()) {
        EditorLayer def = {};
        def.name = "Default";
        def.visible = true;
        st->layers.push_back(def);
    }

    ReadBoolField(json, "viewport2d", &st->viewport2D);
    ReadBoolField(json, "show_left_panel", &st->showLeftPanel);
    ReadBoolField(json, "show_right_panel", &st->showRightPanel);
    ReadBoolField(json, "status_expanded", &st->statusExpanded);
    ReadBoolField(json, "isolate_selected", &st->isolateSelected);

    int leftTab = (int)st->leftTab;
    int filterMode = (int)st->filterMode;
    int gizmoMode = (int)st->gizmoMode;
    ReadIntField(json, "left_tab", &leftTab);
    ReadIntField(json, "filter_mode", &filterMode);
    ReadIntField(json, "gizmo_mode", &gizmoMode);
    st->leftTab = (LeftTab)ClampI(leftTab, 0, 4);
    st->filterMode = (FilterMode)ClampI(filterMode, 0, 2);
    st->gizmoMode = (GizmoMode)ClampI(gizmoMode, 0, 2);

    ReadVec3Field(json, "orbit_target", &st->orbitTarget);
    ReadFloatField(json, "orbit_yaw", &st->orbitYaw);
    ReadFloatField(json, "orbit_pitch", &st->orbitPitch);
    ReadFloatField(json, "orbit_distance", &st->orbitDistance);

    ReadFloatField(json, "camera_orbit_sensitivity", &st->cameraOrbitSensitivity);
    ReadFloatField(json, "camera_pan_sensitivity", &st->cameraPanSensitivity);
    ReadFloatField(json, "camera_zoom_sensitivity", &st->cameraZoomSensitivity);
    ReadFloatField(json, "gizmo_move_sensitivity", &st->gizmoMoveSensitivity);
    ReadFloatField(json, "gizmo_rotate_sensitivity", &st->gizmoRotateSensitivity);
    ReadFloatField(json, "gizmo_scale_sensitivity", &st->gizmoScaleSensitivity);
    ReadFloatField(json, "preview_move_speed", &st->previewMoveSpeed);
    ReadFloatField(json, "preview_look_sensitivity", &st->previewLookSensitivity);
    ReadFloatField(json, "preview_zoom_speed", &st->previewZoomSpeed);
    ReadFloatField(json, "preview_pan_sensitivity", &st->previewPanSensitivity);
    ReadFloatField(json, "vehicle_forward_yaw", &st->vehicleForwardYawDeg);

    st->orbitPitch = ClampF(st->orbitPitch, -1.45f, 1.45f);
    st->orbitDistance = ClampF(st->orbitDistance, 3.0f, 220.0f);
    st->cameraOrbitSensitivity = ClampF(st->cameraOrbitSensitivity, 0.001f, 0.030f);
    st->cameraPanSensitivity = ClampF(st->cameraPanSensitivity, 0.001f, 0.040f);
    st->cameraZoomSensitivity = ClampF(st->cameraZoomSensitivity, 0.010f, 0.200f);
    st->vehicleForwardYawDeg = ClampF(st->vehicleForwardYawDeg, -180.0f, 180.0f);

    for (EditorObject &obj : st->objects) {
        obj.layerIndex = ClampI(obj.layerIndex, 0, (int)st->layers.size() - 1);
        if (obj.parentIndex < -1 || obj.parentIndex >= (int)st->objects.size()) obj.parentIndex = -1;
    }

    st->selectedIndex = st->objects.empty() ? -1 : 0;
    st->selectedIndices.clear();
    if (st->selectedIndex >= 0) st->selectedIndices.push_back(st->selectedIndex);
    st->activeLayerIndex = 0;
    st->selectedShotpoint = st->shotpoints.empty() ? -1 : 0;
    st->renameIndex = -1;
    st->renameFromInspector = false;
    st->colorSyncIndex = -1;

    return true;
}
