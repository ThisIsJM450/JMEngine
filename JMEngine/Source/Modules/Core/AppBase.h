#pragma once
#include <Windows.h>
#include <memory>

#include "Timer.h"
#include "Win32Window.h"
#include "../Game/World.h"
#include "../Graphics//Dx11Context.h"
#include "../Renderer/Renderer.h"
#include "../UI/ImGuiHandler.h"


class MeshLoader;

#define GAssetRegistry AppBase::Get().GetAssetRegistry()

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
    explicit AppBase(const AppDesc& Desc); // 형변환 방지를 위해 explicit
    virtual ~AppBase();

    static const AppBase& Get() {return *s_Instance;}
    int Run();
    
    const Win32Window* GetMainWindow() const { return m_Window.get(); }
    const AssetRegistry* GetAssetRegistry() const { return m_AssetRegistry.get(); }
    
    static AppBase* s_Instance;

private:
    void Tick(float DeltaTime);
    void Render();

private:
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Dx11Context> m_Gfx;
    std::unique_ptr<Renderer>    m_Renderer;
    std::unique_ptr<World>       m_World;
    std::unique_ptr<Timer>       m_Timer;
    std::shared_ptr<Input>       m_Input;   
    std::unique_ptr<ImGuiHandler> m_ImGuiHandler;
    std::shared_ptr<AssetRegistry> m_AssetRegistry;
    


    bool m_Running = true;
};
