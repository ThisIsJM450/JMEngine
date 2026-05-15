#include "AppBase.h"

#include <__msvc_filebuf.hpp>
#include <filesystem>
#include <vector>

#include "../Editor/EditorContext.h"
#include "../Input/EditorViewportInputContext.h"
#include "../Graphics/Mesh/MeshLoader.h"
#include "../Scene/Utils/MeshFactory.h"

AppBase* AppBase::s_Instance = nullptr;

namespace
{
    std::filesystem::path ResolveProjectRootFromWorkingDir()
    {
        namespace fs = std::filesystem;

        fs::path current = fs::current_path();
        while (!current.empty())
        {
            if (fs::exists(current / "Contents") && fs::exists(current / "JMEngine.sln"))
            {
                return current.lexically_normal();
            }

            if (!current.has_parent_path() || current.parent_path() == current)
            {
                break;
            }

            current = current.parent_path();
        }

        return fs::current_path().lexically_normal();
    }
}

AppBase::AppBase(const AppDesc& Desc)
{
    m_Window = std::unique_ptr<Win32Window>(new Win32Window(Desc.hInstance, Desc.Width, Desc.Height, Desc.WindowTitle));
    m_Gfx = std::unique_ptr<Dx11Context>(new Dx11Context(m_Window->GetHwnd(), Desc.Width, Desc.Height));
    m_InputSystem = std::make_shared<InputSystem>();
    m_Window->SetInput(m_InputSystem);
    m_InputRouter.AddContext(std::make_shared<EditorViewportInputContext>());
    m_Timer = std::unique_ptr<Timer>(new Timer());

    m_World = std::unique_ptr<World>(new World());

    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Create(*m_Gfx);
    m_DebugRenderer = std::make_unique<DebugRenderer>();
    m_EditorOverlayRenderer = std::make_unique<EditorOverlayRenderer>();

    // Pipeline / shaders
    m_Gfx->InitBasicPipeline(L"Shader\\ForwardLit.hlsl", L"Shader\\ShadowDepth.hlsl");

    // Bind
    m_Window->SetOnResize([this] (int w, int h)
    {
       if (m_Gfx)
       {
           m_Gfx->Resize(w, h);

           // View는 활성 CameraActor 기준으로 매 프레임 갱신한다.
       }
    });
    
    GEditor.renderer = m_Renderer.get();
    GEditor.world = m_World.get();
    
    m_ImGuiHandler = std::make_unique<ImGuiHandler>();
    m_ImGuiHandler->Init(ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height)), m_Window.get(), m_Gfx.get());
    
    const std::filesystem::path projectRoot = ResolveProjectRootFromWorkingDir();

    m_AssetRegistry = std::make_shared<AssetRegistry>();
    m_AssetRegistry->SetContentRoot(projectRoot / "Contents");
    m_AssetRegistry->SetDatabasePath(projectRoot / "AssetRegistry.json");
    m_AssetRegistry->LoadFromDisk();
    m_AssetRegistry->ScanAll();
    m_AssetRegistry->SaveToDisk();

    m_AssetManager = std::make_unique<AssetManager>(m_AssetRegistry.get());
    m_AssetWatcher = std::make_unique<AssetWatcher>();
    m_AssetWatcher->Start(m_AssetRegistry->GetContentRoot(), 1000);
    
    assert(s_Instance == nullptr); // 무조건 하나만 만들어져야한다.
    s_Instance = this;
}

AppBase::~AppBase()
{
    if (m_AssetWatcher)
    {
        m_AssetWatcher->Stop();
    }

    m_Window.reset();
    m_Gfx.reset();
    m_Renderer.reset();
    m_World.reset();
    m_Timer.reset();
    m_InputSystem.reset();
    m_AssetWatcher.reset();
    
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
    bool shouldSaveRegistry = false;

    if (m_AssetWatcher && m_AssetRegistry)
    {
        const std::vector<AssetWatchEvent> events = m_AssetWatcher->ConsumeEvents();

        for (const AssetWatchEvent& ev : events)
        {
            const std::filesystem::path normalized = ev.absolutePath.lexically_normal();
            if (ev.type == AssetWatchEventType::Remove)
            {
                m_AssetRegistry->RemoveBySourcePath(normalized.string());
                shouldSaveRegistry = true;
            }
            else
            {
                m_AssetRegistry->UpsertFileFromPath(normalized);
                shouldSaveRegistry = true;
            }
        }
    }

    if (shouldSaveRegistry && m_AssetRegistry)
    {
        m_AssetRegistry->SaveToDisk();
    }

    m_World->Tick(DeltaTime);
    if (m_InputSystem)
    {
        m_InputRouter.Route(DeltaTime, *m_InputSystem, *m_World);
    }

    ViewInfo viewInfo = m_World->GetCameraManager().BuildViewInfo(m_Gfx->GetWidth(), m_Gfx->GetHeight());
    m_World->GetScene().SetViewInfo(viewInfo);

    m_Window->Tick(DeltaTime);
    m_ImGuiHandler->Update();
}

void AppBase::Render()
{
    m_Gfx->BeginFrame();
    m_Renderer->RenderRuntimeScene(*m_Gfx, m_World->GetScene());
    m_DebugRenderer->Render(*m_Gfx, *m_Renderer, m_World->GetScene());
    m_EditorOverlayRenderer->Render(*m_Gfx, *m_Renderer, m_World->GetScene());
    m_Renderer->CompositeToBackBuffer(*m_Gfx);

    //IMGUI Render 전
    m_Gfx->BindBackbuffer();
    m_ImGuiHandler->Render();
    m_Gfx->EndFrame();
}
