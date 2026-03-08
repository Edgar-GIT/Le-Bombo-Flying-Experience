#include "../include/persistence_picker.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

// remove espacos no inicio e no fim
static std::string Trim(const std::string &raw) {
    size_t a = 0;
    while (a < raw.size() && std::isspace((unsigned char)raw[a])) a++;

    size_t b = raw.size();
    while (b > a && std::isspace((unsigned char)raw[b - 1])) b--;

    return raw.substr(a, b - a);
}

// executa um comando e devolve a primeira saida de texto
static std::string RunOneCommand(const char *cmd) {
    if (!cmd || !cmd[0]) return "";

#if defined(_WIN32)
    FILE *pipe = _popen(cmd, "r");
#else
    FILE *pipe = popen(cmd, "r");
#endif
    if (!pipe) return "";

    char buffer[1024] = { 0 };
    std::string out;
    if (std::fgets(buffer, (int)sizeof(buffer), pipe)) out = buffer;

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return Trim(out);
}

// tenta abrir seletor nativo de pasta e devolve o caminho
std::string PickFolderPathNative() {
#if defined(__linux__)
    std::string out = RunOneCommand("zenity --file-selection --directory --title='open project folder' 2>/dev/null");
    if (!out.empty()) return out;

    out = RunOneCommand("kdialog --getexistingdirectory 2>/dev/null");
    if (!out.empty()) return out;

    return "";
#elif defined(_WIN32)
    return RunOneCommand("powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; $f = New-Object System.Windows.Forms.FolderBrowserDialog; if($f.ShowDialog() -eq 'OK'){Write-Output $f.SelectedPath}\" 2>NUL");
#elif defined(__APPLE__)
    return RunOneCommand("osascript -e 'set chosenFolder to choose folder with prompt \"open project folder\"' -e 'POSIX path of chosenFolder' 2>/dev/null");
#else
    return "";
#endif
}

// tenta abrir seletor nativo de ficheiro e devolve o caminho
std::string PickFilePathNative(const char *title, const char *pattern) {
    const char *safeTitle = (title && title[0]) ? title : "open file";
    const char *safePattern = (pattern && pattern[0]) ? pattern : "*";

#if defined(__linux__)
    char cmd[1024] = { 0 };
    std::snprintf(cmd, sizeof(cmd),
                  "zenity --file-selection --title='%s' --file-filter='files | %s' 2>/dev/null",
                  safeTitle, safePattern);
    std::string out = RunOneCommand(cmd);
    if (!out.empty()) return out;

    std::snprintf(cmd, sizeof(cmd),
                  "kdialog --getopenfilename . \"%s|files\" 2>/dev/null",
                  safePattern);
    out = RunOneCommand(cmd);
    if (!out.empty()) return out;
    return "";
#elif defined(_WIN32)
    char cmd[2048] = { 0 };
    std::snprintf(cmd, sizeof(cmd),
                  "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; $f = New-Object System.Windows.Forms.OpenFileDialog; $f.Title='%s'; $f.Filter='Files|%s'; if($f.ShowDialog() -eq 'OK'){Write-Output $f.FileName}\" 2>NUL",
                  safeTitle, safePattern);
    return RunOneCommand(cmd);
#elif defined(__APPLE__)
    char cmd[2048] = { 0 };
    std::snprintf(cmd, sizeof(cmd),
                  "osascript -e 'set chosenFile to choose file with prompt \"%s\"' -e 'POSIX path of chosenFile' 2>/dev/null",
                  safeTitle);
    return RunOneCommand(cmd);
#else
    (void)safeTitle;
    (void)safePattern;
    return "";
#endif
}
