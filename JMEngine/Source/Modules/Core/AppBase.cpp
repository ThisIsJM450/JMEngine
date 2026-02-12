#include "AppBase.h"

#include <__msvc_filebuf.hpp>

#include "../Editor/EditorContext.h"
#include "../Graphics/Mesh/MeshLoader.h"
#include "../Scene/Utils/MeshFactory.h"

AppBase* AppBase::s_Instance = nullptr;

AppBase::AppBase(const AppDesc& Desc)
{
    m_Window = std::unique_ptr<Win32Window>(new Win32Window(Desc.hInstance, Desc.Width, Desc.Height, Desc.WindowTitle));
    m_Gfx = std::unique_ptr<Dx11Context>(new Dx11Context(m_Window->GetHwnd(), Desc.Width, Desc.Height));
    m_Input = std::make_shared<Input>(Input());
    m_Window->SetInput(m_Input);
    m_Timer = std::unique_ptr<Timer>(new Timer());

    m_World = std::unique_ptr<World>(new World());
    // Camera
    m_World->GetScene().GetCamera().SetPerspective(60.0f, float(Desc.Width) / float(Desc.Height), 0.1f, 1000.0f);
    m_World->GetScene().GetCamera().LookAt({ 0.0f, 1.3f, -3.2f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });

    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Create(*m_Gfx);

    // Pipeline / shaders
    m_Gfx->InitBasicPipeline(L"Shader\\ForwardLit.hlsl", L"Shader\\ShadowDepth.hlsl");

    // Bind
    m_Window->SetOnResize([this] (int w, int h)
    {
       if (m_Gfx)
       {
           m_Gfx->Resize(w, h);

           // 카메라도 같이 갱신
           if (h > 0)
           {
               float aspect = (float)w / (float)h;
               m_World->GetScene().GetCamera().SetPerspective(60.f, aspect, 0.1f, 1000.f);
           }
       }
    });
    
    GEditor.renderer = m_Renderer.get();
    GEditor.world = m_World.get();
    
    m_ImGuiHandler = std::make_unique<ImGuiHandler>();
    m_ImGuiHandler->Init(ImVec2(Desc.Width, Desc.Height), m_Window.get(), m_Gfx.get());
    
    m_AssetRegistry = std::make_shared<AssetRegistry>();
    m_AssetRegistry->SetContentRoot("D:\\Projects\\JMEngine\\Contents\\");
    m_AssetRegistry->SetDatabasePath("D:\\Projects\\JMEngine\\AssetRegistry.json");
    if (!m_AssetRegistry->LoadFromDisk())
    {
        m_AssetRegistry->ScanAll();
    }
    
    assert(s_Instance == nullptr); // 무조건 하나만 만들어져야한다.
    s_Instance = this;
}

AppBase::~AppBase()
{
    m_Window.reset();
    m_Gfx.reset();
    m_Renderer.reset();
    m_World.reset();
    m_Timer.reset();
    m_Input.reset();
    
    if (s_Instance == this)
    {
        s_Instance = nullptr;
    }
}

int AppBase::Run()
{
    m_World->BeginPlay();
    while (m_Running)
    {
        if (!m_Window->PumpMessages())
        {
            m_World->EndPlay();
            m_Running = false;
            break;
        }

        Tick(m_Timer->Tick());
        Render();
    }

    return 0;
}

void AppBase::Tick(float DeltaTime)
{
    m_World->Tick(DeltaTime);
    m_Input->Tick(DeltaTime, m_World->GetScene());
    m_Window->Tick(DeltaTime);
    m_ImGuiHandler->Update();
}

void AppBase::Render()
{
    m_Gfx->BeginFrame();
    m_Renderer->Render(*m_Gfx, m_World->GetScene());
    
    //IMGUI Render 전
    m_Gfx->BindBackbuffer();
    m_ImGuiHandler->Render();
    m_Gfx->EndFrame();
}
