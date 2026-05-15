#pragma once
#include <Windows.h>
#include <memory>

#include "Timer.h"
#include "Win32Window.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetWatcher.h"
#include "../Game/World.h"
#include "../Graphics//Dx11Context.h"
#include "../Input/Input.h"
#include "../Input/InputRouter.h"
#include "../Renderer/Renderer.h"
#include "../Renderer/DebugRenderer.h"
#include "../Renderer/EditorOverlayRenderer.h"
#include "../UI/ImGuiHandler.h"

class MeshLoader;

#define GAssetRegistry AppBase::Get().GetAssetRegistry()
#define GAssetManager AppBase::Get().GetAssetManager()

struct AppDesc
{
    HINSTANCE hInstance = nullptr;
    int Width = 1280;
    int Height = 720;
    const wchar_t* WindowTitle = L"JMEngine";
};

class AppBase
{
public:
    explicit AppBase(const AppDesc& Desc);
    virtual ~AppBase();

    static const AppBase& Get() { return *s_Instance; }
    int Run();

    const Win32Window* GetMainWindow() const { return m_Window.get(); }
    const AssetRegistry* GetAssetRegistry() const { return m_AssetRegistry.get(); }
    AssetManager* GetAssetManager() const { return m_AssetManager.get(); }

    static AppBase* s_Instance;

private:
    void Tick(float DeltaTime);
    void Render();

private:
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Dx11Context> m_Gfx;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<DebugRenderer> m_DebugRenderer;
    std::unique_ptr<EditorOverlayRenderer> m_EditorOverlayRenderer;
    std::unique_ptr<World> m_World;
    std::unique_ptr<Timer> m_Timer;
    std::shared_ptr<InputSystem> m_InputSystem;
    InputRouter m_InputRouter;
    std::unique_ptr<ImGuiHandler> m_ImGuiHandler;
    std::shared_ptr<AssetRegistry> m_AssetRegistry;
    std::unique_ptr<AssetManager> m_AssetManager;
    std::unique_ptr<AssetWatcher> m_AssetWatcher;

    bool m_Running = true;
};
