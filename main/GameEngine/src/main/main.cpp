#include "../include/main.hpp"
#include "../include/gui_manager.hpp"

#include <chrono>
#include "raylib.h"

// initializes base engine flags before lifecycle start
EngineBase::EngineBase() : running_(false), initialized_(false) {}

// boots the engine once and marks the run loop as active
bool EngineBase::Init() {
    if (initialized_) {
        running_ = true;
        return true;
    }

    if (!OnInit()) {
        return false;
    }

    initialized_ = true;
    running_ = true;
    return true;
}

// executes the fixed lifecycle loop and computes per-frame delta time
void EngineBase::Run() {
    if (!initialized_ && !Init()) {
        return;
    }

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();

    while (running_) {
        const auto now = Clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        OnUpdate(dt);
        OnRender();
    }

    if (initialized_) {
        Shutdown();
    }
}

// releases engine resources and closes lifecycle state
void EngineBase::Shutdown() {
    if (!initialized_) {
        return;
    }

    running_ = false;
    OnShutdown();
    initialized_ = false;
}

// requests loop termination on next iteration
void EngineBase::RequestQuit() { running_ = false; }

// exposes current run loop state
bool EngineBase::IsRunning() const { return running_; }

// provides default startup hook behavior
bool EngineBase::OnInit() { return true; }

// provides default update hook behavior
void EngineBase::OnUpdate(float) {}

// provides default render hook behavior
void EngineBase::OnRender() {}

// provides default shutdown hook behavior
void EngineBase::OnShutdown() {}

class GameEngineApp : public EngineBase {
protected:
    // creates and configures the editor window
    bool OnInit() override {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(1280, 720, "Le Bombo Game Engine");
        MaximizeWindow();
        SetTargetFPS(60);
        return true;
    }

    // handles frame-level quit input checks
    void OnUpdate(float) override {
        if (WindowShouldClose()) {
            RequestQuit();
        }
    }

    // renders the engine gui every frame
    void OnRender() override {
        BeginDrawing();
            ClearBackground(BLACK);
            DrawEngineGuiLayout(GetFrameTime());
        EndDrawing();
    }

    // safely closes the window during shutdown
    void OnShutdown() override {
        ShutdownEngineGuiLayout();
        if (IsWindowReady()) {
            CloseWindow();
        }
    }
};

// starts the engine app lifecycle
int main() {
    GameEngineApp app;
    app.Run();
    return 0;
}
