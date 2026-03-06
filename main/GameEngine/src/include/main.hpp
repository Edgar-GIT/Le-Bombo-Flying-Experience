#ifndef GAMEENGINE_MAIN_HPP
#define GAMEENGINE_MAIN_HPP

class EngineBase {
public:
    EngineBase();
    virtual ~EngineBase() = default;

    bool Init();
    void Run();
    void Shutdown();
    void RequestQuit();
    bool IsRunning() const;

protected:
    virtual bool OnInit();
    virtual void OnUpdate(float dt);
    virtual void OnRender();
    virtual void OnShutdown();

private:
    bool running_;
    bool initialized_;
};

#endif
