#include "../include/persistence.hpp"
#include "../include/gui_manager.hpp"
#include "../include/gui_workflow.hpp"
#include "../include/piece_library.hpp"
#include "../include/persistence_io.hpp"
#include "../include/persistence_picker.hpp"
#include "../include/workspace_tabs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// guarda se o campo de texto do dialogo esta ativo
static bool gProjectInputActive = false;

// resolve a raiz do repositorio
static std::filesystem::path ResolveProjectRootPath() {
    std::filesystem::path current = std::filesystem::current_path();
    while (true) {
        if (std::filesystem::exists(current / "main" / "src" / "main.c")) return current;
        std::filesystem::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return std::filesystem::current_path();
}

// remove caracteres invalidos do nome do projeto
static std::string SanitizeProjectName(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());

    for (char c : raw) {
        unsigned char u = (unsigned char)c;
        if (std::isalnum(u) || c == '_' || c == '-') out.push_back(c);
        else if (c == ' ') out.push_back('_');
    }

    if (out.empty()) out = "project";
    return out;
}

// cria a pasta base dos projetos
static std::filesystem::path EnsureProjectsRoot(EditorGuiState &st) {
    if (!st.projectsRootPath.empty()) {
        std::filesystem::path p = st.projectsRootPath;
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }

    std::filesystem::path root = ResolveProjectRootPath() / "main" / "GameEngine" / "projects";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    st.projectsRootPath = root.string();
    return root;
}

// devolve caminho do ficheiro scene json
static std::filesystem::path SceneFilePath(const std::filesystem::path &projectPath) {
    return projectPath / "scene.json";
}

// devolve caminho do ficheiro autosave json
static std::filesystem::path AutosaveFilePath(const std::filesystem::path &projectPath) {
    return projectPath / "autosave.json";
}

// devolve caminho do descritor de recovery
static std::filesystem::path RecoveryDescriptorPath() {
    std::filesystem::path root = ResolveProjectRootPath();
    std::filesystem::path cacheDir = root / "main" / "GameEngine" / "cache";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);
    return cacheDir / "recovery.json";
}

// atualiza a lista de pastas de projeto
static void RefreshProjectList(EditorGuiState &st) {
    st.projectFolderList.clear();

    std::filesystem::path projectsRoot = EnsureProjectsRoot(st);
    std::error_code ec;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(projectsRoot, ec)) {
        if (entry.is_directory()) st.projectFolderList.push_back(entry.path().filename().string());
    }

    std::sort(st.projectFolderList.begin(), st.projectFolderList.end());
}

// limpa o descritor de recovery
static void ClearRecoveryDescriptor() {
    std::error_code ec;
    std::filesystem::remove(RecoveryDescriptorPath(), ec);
}

// grava descritor de recovery com a pasta ativa
static void WriteRecoveryDescriptor(const std::filesystem::path &projectPath) {
    std::ofstream out(RecoveryDescriptorPath(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"project_path\":\"" << projectPath.string() << "\",\n";
    out << "  \"has_autosave\":true\n";
    out << "}\n";
}

// le descritor de recovery
static bool ReadRecoveryDescriptor(std::string *outProjectPath) {
    std::ifstream in(RecoveryDescriptorPath(), std::ios::in);
    if (!in.is_open()) return false;

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string data = ss.str();

    std::string token = "\"project_path\":\"";
    size_t pos = data.find(token);
    if (pos == std::string::npos) return false;
    pos += token.size();

    size_t end = data.find('"', pos);
    if (end == std::string::npos || end <= pos) return false;

    *outProjectPath = data.substr(pos, end - pos);
    return true;
}

// aplica o estado basico apos abrir projeto
static void ApplyProjectLoadedState(EditorGuiState &st, const std::string &projectName, const std::filesystem::path &projectPath) {
    st.currentProjectName = projectName;
    st.currentProjectPath = projectPath.string();
    st.projectLoaded = true;
    st.projectDirty = false;
    st.autosaveLastTime = GetTime();

    st.showProjectDialog = false;
    st.projectDialogMode = ProjectDialogMode::None;
    st.showUnsavedConfirm = false;
    st.pendingDialogMode = ProjectDialogMode::None;
    st.projectErrorBuffer[0] = '\0';
    st.projectListScroll = 0.0f;
    gProjectInputActive = false;

    ResetWorkspacesForProject(st);
    ReloadPieceLibrary(st);
    ResetHistoryBaseline(st);
    UpdateCameraFromOrbit(st);
}

// abre projeto a partir do caminho e escolhe scene ou autosave
static bool OpenProjectPath(EditorGuiState &st, const std::filesystem::path &projectPath, bool useAutosave) {
    std::error_code ec;
    if (!std::filesystem::exists(projectPath, ec) || !std::filesystem::is_directory(projectPath, ec)) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "project folder not found");
        return false;
    }

    std::filesystem::path scene = useAutosave ? AutosaveFilePath(projectPath) : SceneFilePath(projectPath);
    if (!std::filesystem::exists(scene, ec)) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "scene json not found in folder");
        return false;
    }

    if (!LoadProjectJsonFile(scene, &st)) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "failed to parse scene json");
        return false;
    }

    for (Shotpoint &sp : st.shotpoints) {
        if (sp.objectIndex < 0 || sp.objectIndex >= (int)st.objects.size()) sp.objectIndex = -1;
    }

    std::string projectName = projectPath.filename().string();
    if (projectName.empty()) projectName = "project";
    ApplyProjectLoadedState(st, projectName, projectPath);
    if (useAutosave) st.projectDirty = true;

    char msg[196] = { 0 };
    std::snprintf(msg, sizeof(msg), useAutosave ? "project recovered %s" : "project opened %s", projectName.c_str());
    AddLog(st, msg);
    return true;
}

// cria um projeto novo e abre esse projeto
static bool CreateNewProjectFromInput(EditorGuiState &st, const std::string &rawName) {
    std::string safeName = SanitizeProjectName(rawName);
    std::filesystem::path root = EnsureProjectsRoot(st);
    std::filesystem::path projectPath = root / safeName;

    std::error_code ec;
    if (std::filesystem::exists(projectPath, ec)) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "project folder already exists");
        return false;
    }

    std::filesystem::create_directories(projectPath, ec);
    if (ec) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "failed to create project folder");
        return false;
    }

    st.objects.clear();
    st.shotpoints.clear();
    st.selectedIndices.clear();
    st.layers.clear();
    EditorLayer defaultLayer = {};
    defaultLayer.name = "Default";
    defaultLayer.visible = true;
    st.layers.push_back(defaultLayer);
    st.activeLayerIndex = 0;
    st.selectedIndex = -1;
    st.selectedShotpoint = -1;
    st.renameIndex = -1;
    st.renameFromInspector = false;
    st.colorSyncIndex = -1;
    st.viewport2D = false;
    st.showLeftPanel = true;
    st.showRightPanel = true;
    st.statusExpanded = false;
    st.isolateSelected = false;

    ApplyProjectLoadedState(st, safeName, projectPath);
    st.projectDirty = true;

    if (!SaveCurrentProject(st, false)) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "failed to write project json");
        return false;
    }

    char msg[196] = { 0 };
    std::snprintf(msg, sizeof(msg), "project created %s", safeName.c_str());
    AddLog(st, msg);
    RefreshProjectList(st);
    return true;
}

// resolve a pasta do projeto a partir do texto digitado
static std::filesystem::path ResolveProjectFolderFromInput(EditorGuiState &st, const std::string &input) {
    std::filesystem::path folderInput = input;

    if (folderInput.is_absolute()) return folderInput;

    std::string text = input;
    if (text.find('/') != std::string::npos || text.find('\\') != std::string::npos || (text.size() > 1 && text[0] == '.')) {
        return std::filesystem::path(text);
    }

    return EnsureProjectsRoot(st) / text;
}

// abre um projeto existente e carrega a cena
static bool OpenProjectFromInput(EditorGuiState &st, const std::string &rawInput) {
    if (rawInput.empty()) {
        std::snprintf(st.projectErrorBuffer, sizeof(st.projectErrorBuffer), "insert a project folder");
        return false;
    }

    return OpenProjectPath(st, ResolveProjectFolderFromInput(st, rawInput), false);
}

// abre dialogo new ou open sem passar pelo fluxo de dirty
static void StartProjectDialog(EditorGuiState &st, ProjectDialogMode mode) {
    RefreshProjectList(st);
    st.showProjectDialog = true;
    st.projectDialogMode = mode;
    st.projectNameBuffer[0] = '\0';
    st.projectErrorBuffer[0] = '\0';
    st.projectListScroll = 0.0f;
    gProjectInputActive = true;
}

// decide se abre dialogo direto ou pede confirmacao de dirty
static void RequestProjectDialog(EditorGuiState &st, ProjectDialogMode mode) {
    if (st.projectLoaded && st.projectDirty) {
        st.showUnsavedConfirm = true;
        st.pendingDialogMode = mode;
        return;
    }

    StartProjectDialog(st, mode);
}

// desenha modal de confirmacao para projeto dirty
static void DrawUnsavedConfirm(EditorGuiState &st) {
    if (!st.showUnsavedConfirm) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 140 });
    Rectangle box = { sw * 0.5f - 220.0f, sh * 0.5f - 90.0f, 440.0f, 180.0f };
    DrawRectangleRounded(box, 0.08f, 8, (Color){ 34, 38, 46, 255 });
    DrawRectangleRoundedLinesEx(box, 0.08f, 8, 1.4f, (Color){ 98, 108, 126, 255 });

    DrawText("unsaved changes", (int)box.x + 14, (int)box.y + 12, 24, (Color){ 232, 236, 243, 255 });
    DrawText("you have changes not saved in scene json", (int)box.x + 14, (int)box.y + 52, 18, (Color){ 205, 212, 224, 255 });

    bool prevBlock = gBlockUiInput;
    gBlockUiInput = false;

    Rectangle cancelBtn = { box.x + 14.0f, box.y + box.height - 42.0f, 104.0f, 28.0f };
    Rectangle continueBtn = { box.x + 126.0f, box.y + box.height - 42.0f, 144.0f, 28.0f };
    Rectangle saveContinueBtn = { box.x + box.width - 154.0f, box.y + box.height - 42.0f, 140.0f, 28.0f };

    if (DrawButton(cancelBtn, "cancel", "cancel action")) {
        st.showUnsavedConfirm = false;
        st.pendingDialogMode = ProjectDialogMode::None;
    }

    if (DrawButton(continueBtn, "continue", "continue without save")) {
        ProjectDialogMode mode = st.pendingDialogMode;
        st.showUnsavedConfirm = false;
        st.pendingDialogMode = ProjectDialogMode::None;
        StartProjectDialog(st, mode);
    }

    if (DrawButton(saveContinueBtn, "save continue", "save and continue")) {
        if (SaveCurrentProject(st, false)) {
            ProjectDialogMode mode = st.pendingDialogMode;
            st.showUnsavedConfirm = false;
            st.pendingDialogMode = ProjectDialogMode::None;
            StartProjectDialog(st, mode);
        }
    }

    gBlockUiInput = prevBlock;
}

// desenha modal de recovery no arranque
static void DrawRecoveryDialog(EditorGuiState &st) {
    if (!st.showRecoveryDialog) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 140 });
    Rectangle box = { sw * 0.5f - 250.0f, sh * 0.5f - 92.0f, 500.0f, 184.0f };
    DrawRectangleRounded(box, 0.08f, 8, (Color){ 34, 38, 46, 255 });
    DrawRectangleRoundedLinesEx(box, 0.08f, 8, 1.4f, (Color){ 98, 108, 126, 255 });

    DrawText("recovery found", (int)box.x + 14, (int)box.y + 12, 24, (Color){ 232, 236, 243, 255 });
    DrawText("autosave from previous session was found", (int)box.x + 14, (int)box.y + 52, 18, (Color){ 205, 212, 224, 255 });
    DrawText(st.recoveryProjectPath.c_str(), (int)box.x + 14, (int)box.y + 76, 16, (Color){ 171, 182, 198, 255 });

    bool prevBlock = gBlockUiInput;
    gBlockUiInput = false;

    Rectangle discardBtn = { box.x + 14.0f, box.y + box.height - 42.0f, 116.0f, 28.0f };
    Rectangle laterBtn = { box.x + 138.0f, box.y + box.height - 42.0f, 116.0f, 28.0f };
    Rectangle recoverBtn = { box.x + box.width - 126.0f, box.y + box.height - 42.0f, 112.0f, 28.0f };

    if (DrawButton(discardBtn, "discard", "discard recovery data")) {
        st.showRecoveryDialog = false;
        st.recoveryProjectPath.clear();
        ClearRecoveryDescriptor();
        AddLog(st, "recovery discarded");
    }

    if (DrawButton(laterBtn, "later", "close for now")) {
        st.showRecoveryDialog = false;
    }

    if (DrawButton(recoverBtn, "recover", "load autosave")) {
        if (OpenProjectPath(st, std::filesystem::path(st.recoveryProjectPath), true)) {
            st.showRecoveryDialog = false;
            st.recoveryProjectPath.clear();
            ClearRecoveryDescriptor();
        }
    }

    gBlockUiInput = prevBlock;
}

// desenha o dialogo de new open save
static void DrawProjectOpenNewDialog(EditorGuiState &st) {
    if (!st.showProjectDialog) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 140 });

    Rectangle box = { sw * 0.5f - 300.0f, sh * 0.5f - 220.0f, 600.0f, 440.0f };
    DrawRectangleRounded(box, 0.06f, 8, (Color){ 34, 38, 46, 255 });
    DrawRectangleRoundedLinesEx(box, 0.06f, 8, 1.2f, (Color){ 98, 108, 126, 255 });

    bool isNew = st.projectDialogMode == ProjectDialogMode::NewProject;
    const char *title = isNew ? "new project" : "open project";
    const char *hint = isNew ? "type project name" : "type folder name click list or pick";

    DrawText(title, (int)box.x + 14, (int)box.y + 12, 24, (Color){ 232, 236, 243, 255 });
    DrawText(hint, (int)box.x + 14, (int)box.y + 46, 18, (Color){ 185, 194, 208, 255 });

    Rectangle input = { box.x + 14.0f, box.y + 74.0f, box.width - (isNew ? 28.0f : 116.0f), 30.0f };
    bool inputHover = CheckCollisionPointRec(GetMousePosition(), input);
    if (inputHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) gProjectInputActive = true;

    DrawRectangleRounded(input, 0.10f, 6, (Color){ 30, 35, 43, 255 });
    DrawRectangleRoundedLinesEx(input, 0.10f, 6, 1.0f, gProjectInputActive ? (Color){ 125, 160, 235, 255 } : (Color){ 84, 92, 108, 255 });
    DrawText(st.projectNameBuffer, (int)input.x + 8, (int)input.y + 6, 18, (Color){ 232, 236, 243, 255 });

    bool prevBlock = gBlockUiInput;
    gBlockUiInput = false;
    if (!isNew) {
        Rectangle pickBtn = { input.x + input.width + 8.0f, input.y, 80.0f, 30.0f };
        if (DrawButton(pickBtn, "pick", "open native folder picker")) {
            std::string picked = PickFolderPathNative();
            if (!picked.empty()) {
                std::snprintf(st.projectNameBuffer, sizeof(st.projectNameBuffer), "%s", picked.c_str());
                gProjectInputActive = true;
            }
        }
    }
    gBlockUiInput = prevBlock;

    if (gProjectInputActive) {
        bool submit = false;
        bool cancel = false;
        UpdateTextFieldBuffer(st.projectNameBuffer, sizeof(st.projectNameBuffer), &submit, &cancel);
        if (cancel) {
            st.showProjectDialog = false;
            st.projectDialogMode = ProjectDialogMode::None;
            gProjectInputActive = false;
            return;
        }

        if (submit) {
            bool ok = false;
            if (isNew) ok = CreateNewProjectFromInput(st, st.projectNameBuffer);
            else ok = OpenProjectFromInput(st, st.projectNameBuffer);
            if (ok) return;
        }
    }

    if (!isNew) {
        Rectangle listRect = { box.x + 14.0f, box.y + 114.0f, box.width - 28.0f, box.height - 176.0f };
        DrawRectangleRounded(listRect, 0.05f, 6, (Color){ 29, 33, 40, 255 });
        DrawRectangleRoundedLinesEx(listRect, 0.05f, 6, 1.0f, (Color){ 78, 86, 101, 255 });

        if (CheckCollisionPointRec(GetMousePosition(), listRect)) {
            st.projectListScroll -= GetMouseWheelMove() * 26.0f;
            if (st.projectListScroll < 0.0f) st.projectListScroll = 0.0f;
        }

        BeginScissorMode((int)listRect.x + 1, (int)listRect.y + 1, (int)listRect.width - 2, (int)listRect.height - 2);
        float y = listRect.y + 6.0f - st.projectListScroll;
        for (size_t i = 0; i < st.projectFolderList.size(); i++) {
            const std::string &name = st.projectFolderList[i];
            Rectangle row = { listRect.x + 6.0f, y, listRect.width - 12.0f, 26.0f };
            bool hover = CheckCollisionPointRec(GetMousePosition(), row);
            DrawRectangleRounded(row, 0.12f, 5, hover ? (Color){ 66, 74, 89, 255 } : (Color){ 45, 50, 60, 255 });
            DrawRectangleRoundedLinesEx(row, 0.12f, 5, 1.0f, (Color){ 83, 92, 108, 255 });
            DrawText(name.c_str(), (int)row.x + 8, (int)row.y + 5, 17, (Color){ 226, 232, 242, 255 });

            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                std::snprintf(st.projectNameBuffer, sizeof(st.projectNameBuffer), "%s", name.c_str());
                gProjectInputActive = true;
            }

            y += 30.0f;
        }
        EndScissorMode();
    }

    if (st.projectErrorBuffer[0] != '\0') {
        DrawText(st.projectErrorBuffer, (int)box.x + 14, (int)box.y + box.height - 58, 17, (Color){ 255, 146, 146, 255 });
    }

    prevBlock = gBlockUiInput;
    gBlockUiInput = false;

    Rectangle cancelBtn = { box.x + 14.0f, box.y + box.height - 40.0f, 130.0f, 28.0f };
    Rectangle okBtn = { box.x + box.width - 144.0f, box.y + box.height - 40.0f, 130.0f, 28.0f };

    if (DrawButton(cancelBtn, "cancel", "close dialog")) {
        st.showProjectDialog = false;
        st.projectDialogMode = ProjectDialogMode::None;
        gProjectInputActive = false;
    }

    if (DrawButton(okBtn, isNew ? "create" : "open", isNew ? "create project folder" : "open project folder")) {
        bool ok = false;
        if (isNew) ok = CreateNewProjectFromInput(st, st.projectNameBuffer);
        else ok = OpenProjectFromInput(st, st.projectNameBuffer);
        if (ok) {
            gBlockUiInput = prevBlock;
            return;
        }
    }

    gBlockUiInput = prevBlock;
}

// inicia dados base de persistencia
void InitProjectPersistence(EditorGuiState &st) {
    EnsureProjectsRoot(st);
    if (st.currentProjectName.empty()) st.currentProjectName = "no project";

    st.showProjectDialog = false;
    st.projectDialogMode = ProjectDialogMode::None;
    st.showUnsavedConfirm = false;
    st.pendingDialogMode = ProjectDialogMode::None;
    st.showRecoveryDialog = false;
    st.recoveryProjectPath.clear();
    st.projectNameBuffer[0] = '\0';
    st.projectErrorBuffer[0] = '\0';
    st.projectListScroll = 0.0f;

    st.currentProjectPath.clear();
    st.projectLoaded = false;
    st.projectDirty = false;
    st.autosaveIntervalSec = 4.0;
    st.autosaveLastTime = GetTime();

    RefreshProjectList(st);

    std::string recoveredPath;
    if (ReadRecoveryDescriptor(&recoveredPath)) {
        std::filesystem::path projectPath = recoveredPath;
        if (std::filesystem::exists(AutosaveFilePath(projectPath)) && std::filesystem::exists(projectPath)) {
            st.showRecoveryDialog = true;
            st.recoveryProjectPath = projectPath.string();
        } else {
            ClearRecoveryDescriptor();
        }
    }
}

// fecha estado local de persistencia
void ShutdownProjectPersistence(EditorGuiState &st) {
    st.showProjectDialog = false;
    st.projectDialogMode = ProjectDialogMode::None;
    st.showUnsavedConfirm = false;
    st.pendingDialogMode = ProjectDialogMode::None;
    st.showRecoveryDialog = false;
    st.recoveryProjectPath.clear();
    gProjectInputActive = false;
    ClearRecoveryDescriptor();
}

// pede abertura de dialogo new com validacao de dirty
void OpenNewProjectDialog(EditorGuiState &st) {
    RequestProjectDialog(st, ProjectDialogMode::NewProject);
}

// pede abertura de dialogo open com validacao de dirty
void OpenExistingProjectDialog(EditorGuiState &st) {
    RequestProjectDialog(st, ProjectDialogMode::OpenProject);
}

// grava o projeto atual em scene json ou autosave json
bool SaveCurrentProject(EditorGuiState &st, bool autosave) {
    if (!st.projectLoaded || st.currentProjectPath.empty()) return false;

    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size() &&
        st.workspaces[st.activeWorkspaceIndex].kind == WorkspaceKind::Project) {
        SyncActiveWorkspace(st);
    }

    std::filesystem::path projectPath = st.currentProjectPath;
    std::error_code ec;
    std::filesystem::create_directories(projectPath, ec);

    std::filesystem::path filePath = autosave ? AutosaveFilePath(projectPath) : SceneFilePath(projectPath);
    if (!SaveProjectJsonFile(filePath, st)) return false;

    st.autosaveLastTime = GetTime();
    if (!autosave) {
        st.projectDirty = false;
        ClearRecoveryDescriptor();
        if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size() &&
            st.workspaces[st.activeWorkspaceIndex].kind == WorkspaceKind::Project) {
            st.workspaces[st.activeWorkspaceIndex].dirty = false;
        }

        char msg[196] = { 0 };
        std::snprintf(msg, sizeof(msg), "project saved %s", st.currentProjectName.c_str());
        AddLog(st, msg);
        RecordCacheAction(st, "save", msg);
    } else {
        WriteRecoveryDescriptor(projectPath);
        RecordCacheAction(st, "autosave", "project autosaved");
    }

    return true;
}

// ativa estado dirty quando a cena muda
void MarkProjectDirty(EditorGuiState &st) {
    if (!st.projectLoaded) return;
    if (st.activeWorkspaceIndex >= 0 && st.activeWorkspaceIndex < (int)st.workspaces.size() &&
        st.workspaces[st.activeWorkspaceIndex].kind != WorkspaceKind::Project) {
        st.workspaces[st.activeWorkspaceIndex].dirty = true;
        return;
    }
    st.projectDirty = true;
}

// atualiza autosave por intervalo de tempo
void UpdateProjectAutosave(EditorGuiState &st) {
    if (!st.projectLoaded || !st.projectDirty) return;

    double now = GetTime();
    if ((now - st.autosaveLastTime) < st.autosaveIntervalSec) return;

    SaveCurrentProject(st, true);
}

// desenha todos os popups de persistencia
void DrawProjectDialog(EditorGuiState &st) {
    if (st.showUnsavedConfirm) {
        DrawUnsavedConfirm(st);
        return;
    }

    if (st.showRecoveryDialog) {
        DrawRecoveryDialog(st);
        return;
    }

    DrawProjectOpenNewDialog(st);
}
