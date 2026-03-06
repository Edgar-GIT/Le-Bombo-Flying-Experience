#include "main.hpp"

#include <chrono>

EngineBase::EngineBase() : running_(false), initialized_(false) {}

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

void EngineBase::Shutdown() {
    if (!initialized_) {
        return;
    }

    running_ = false;
    OnShutdown();
    initialized_ = false;
}

void EngineBase::RequestQuit() { running_ = false; }

bool EngineBase::IsRunning() const { return running_; }

bool EngineBase::OnInit() { return true; }

void EngineBase::OnUpdate(float) {}

void EngineBase::OnRender() {}

void EngineBase::OnShutdown() {}
