#include "../include/vehicle_export.hpp"
#include "../include/gui_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

// resolve repository root folder
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

// keep only safe filename chars
static std::string SanitizeName(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u) || c == '_' || c == '-') out.push_back(c);
        else if (c == ' ') out.push_back('_');
    }
    if (out.empty()) out = "vehicle";
    return out;
}

// write one object line in compact export format
static void WriteObjectLine(std::ofstream &out, const EditorObject &obj, Vector3 centerOffset) {
    Vector3 pos = {
        obj.position.x - centerOffset.x,
        obj.position.y - centerOffset.y,
        obj.position.z - centerOffset.z
    };
    out << "obj="
        << (int)obj.kind << ","
        << (obj.visible ? 1 : 0) << ","
        << (obj.is2D ? 1 : 0) << ","
        << pos.x << "," << pos.y << "," << pos.z << ","
        << obj.rotation.x << "," << obj.rotation.y << "," << obj.rotation.z << ","
        << obj.scale.x << "," << obj.scale.y << "," << obj.scale.z << ","
        << (int)obj.color.r << "," << (int)obj.color.g << "," << (int)obj.color.b << "," << (int)obj.color.a
        << "\n";
}

// write one shotpoint line in compact export format
static void WriteShotpointLine(std::ofstream &out, const Shotpoint &sp) {
    out << "sp="
        << sp.objectIndex << ","
        << (sp.enabled ? 1 : 0) << ","
        << sp.localPos.x << "," << sp.localPos.y << "," << sp.localPos.z << ","
        << (int)sp.blastColor.r << "," << (int)sp.blastColor.g << "," << (int)sp.blastColor.b << "," << (int)sp.blastColor.a << ","
        << sp.blastSize
        << "\n";
}

// export scene and shotpoints as .vehicle file for runtime loading
bool ExportCurrentVehicle(EditorGuiState &st) {
    if (!st.projectLoaded || st.currentProjectPath.empty()) {
        AddLog(st, "export unavailable open or create a project first");
        return false;
    }

    if (st.objects.empty()) {
        AddLog(st, "export unavailable add at least one block");
        return false;
    }

    const std::filesystem::path root = ResolveProjectRootPath();
    const std::filesystem::path buildDir = root / "main" / "GameEngine" / "builds";
    std::error_code ec;
    std::filesystem::create_directories(buildDir, ec);
    if (ec) {
        AddLog(st, "export failed could not create builds folder");
        return false;
    }

    const std::string safeName = SanitizeName(st.currentProjectName);
    const std::string displayName = st.currentProjectName.empty() ? safeName : st.currentProjectName;
    const std::filesystem::path outPath = buildDir / (safeName + ".vehicle");
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        AddLog(st, "export failed could not open output file");
        return false;
    }

    out << "LBFE_VEHICLE_V1\n";
    out << "vehicle_name=" << displayName << "\n";
    out << "forward_yaw_deg=" << st.vehicleForwardYawDeg << "\n";
    out << "object_count=" << st.objects.size() << "\n";

    Vector3 center = { 0.0f, 0.0f, 0.0f };
    for (const EditorObject &obj : st.objects) {
        center.x += obj.position.x;
        center.y += obj.position.y;
        center.z += obj.position.z;
    }
    if (!st.objects.empty()) {
        const float inv = 1.0f / (float)st.objects.size();
        center.x *= inv;
        center.y *= inv;
        center.z *= inv;
    }

    for (const EditorObject &obj : st.objects) {
        WriteObjectLine(out, obj, center);
    }

    out << "shotpoint_count=" << st.shotpoints.size() << "\n";
    for (const Shotpoint &sp : st.shotpoints) {
        WriteShotpointLine(out, sp);
    }
    out << "end\n";

    char msg[256] = { 0 };
    std::snprintf(msg, sizeof(msg), "vehicle exported %s", outPath.filename().string().c_str());
    AddLog(st, msg);
    RecordCacheAction(st, "export", outPath.string().c_str());
    return true;
}
