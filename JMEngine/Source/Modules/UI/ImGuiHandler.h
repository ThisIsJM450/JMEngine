#pragma once
#include <imgui.h>

#include "ContentBrouwserPanel.h"

class ActorComponent;
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
    // void UpdateGUI();
    void UpdateTabs();
    
    void Render();
    void RenderViewportPanel();
    void RenderViewportContents();
    void RenderOutlinerPanel();
    void RenderOutlinerPanelContents();
    void RenderDetailsPanel();
    void RenderDetailsPanelContents();
    void RenderComponentDetails(ActorComponent* component);
    void RenderContentBrowserPanel();
    void RenderContentBrowserContents();
    void SetupImGuiStyle_Minimal();
    void SetupImGuiColors_Dark();
    void OnDropToViewport(AssetID id, const AssetMeta& meta);

    int GetGUIWidth() { return m_guiWidth; }
    void RenderWorldActors(World* world);
    void RenderRendererInfo(Renderer* renderer);
    
private:
    int m_guiWidth = 0;
    
    ContentBrowserPanel m_contentBrowserPanel;
};
