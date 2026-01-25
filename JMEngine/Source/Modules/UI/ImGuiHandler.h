#pragma once
#include <imgui.h>

class World;
class Dx11Context;
class Win32Window;
class Renderer;

class ImGuiHandler
{
public:
    ImGuiHandler();
    ~ImGuiHandler();
    
    bool Init(const ImVec2& ScreenBox, Win32Window* Window, Dx11Context* Gfx);
    void ShoutDown();
    
    void Update();
    void UpdateGUI();
    
    void Render();
    
    int GetGUIWidth() { return m_guiWidth; }
    void RenderWorldActors(World* world);
    void RenderRendererInfo(Renderer* renderer);
    
    World* world = nullptr;
    Renderer* renderer = nullptr;
    
private:
    int m_guiWidth = 0;
};
