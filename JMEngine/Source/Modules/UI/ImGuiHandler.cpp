#include "ImGuiHandler.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shobjidl.h>

#include "../Core/AppBase.h"
#include "../Core/Win32Window.h"
#include "../Graphics/Dx11Context.h"
#include "../Core/Settings/RenderSettings.h"
#include "../Game/World.h"
#include "../Game/Actors/DirectionalLightActor.h"
#include "../Game/Actors/StaticMeshActor.h"
#include "../Graphics/Importer/FBXImporter.h"
#include "../Renderer/Renderer.h"


class StaticMeshActor;

namespace 
{
    static std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(sz, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), sz);
        return out;
    }

    static std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty()) return {};
        int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(sz, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), sz, nullptr, nullptr);
        return out;
    }

    // 폴더 선택 다이얼로그
    static bool OpenFileDialog_FBX(HWND ownerHwnd, std::wstring& outPath, const std::wstring& initialDir = L"")
    {
        outPath.clear();

        // 엔진에서 COM을 이미 초기화했다면 여기 CoInitializeEx는 제거하는 게 더 안전
        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool didInit = SUCCEEDED(hrInit);

        IFileOpenDialog* pDlg = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
        if (FAILED(hr) || !pDlg)
        {
            if (didInit) CoUninitialize();
            return false;
        }

        // 필터: *.fbx
        COMDLG_FILTERSPEC rgSpec[] =
        {
            { L"FBX Files (*.fbx)", L"*.fbx" },
            { L"All Files (*.*)",   L"*.*"   }
        };
        pDlg->SetFileTypes(_countof(rgSpec), rgSpec);
        pDlg->SetFileTypeIndex(1);
        pDlg->SetTitle(L"Select FBX file");

        // 초기 디렉토리 설정(옵션)
        if (!initialDir.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(initialDir.c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem)
            {
                pDlg->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        hr = pDlg->Show(ownerHwnd);
        if (SUCCEEDED(hr))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem)) && pItem)
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                {
                    outPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }

        pDlg->Release();
        if (didInit) CoUninitialize();

        return !outPath.empty();
    }
}

ImGuiHandler::ImGuiHandler()
{
}

ImGuiHandler::~ImGuiHandler()
{
    ShoutDown();
}

bool ImGuiHandler::Init(const ImVec2& ScreenBox, Win32Window* Window, Dx11Context* Gfx)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.DisplaySize = ScreenBox;
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplDX11_Init(Gfx->GetDevice(), Gfx->GetContext())) {
        return false;
    }

    if (!ImGui_ImplWin32_Init(Window->GetHwnd())) {
        return false;
    }

    return true;
}

void ImGuiHandler::ShoutDown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}


void ImGuiHandler::Update()
{
    ImGui_ImplDX11_NewFrame(); // GUI 프레임 시작
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame(); // 어떤 것들을 렌더링 할지 기록 시작
    ImGui::Begin("Scene Control");

    // ImGui가 측정해주는 Framerate 출력
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);

    UpdateGUI(); // 추가적으로 사용할 GUI

    ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));

    m_guiWidth = int(ImGui::GetWindowWidth());

    ImGui::End();
    ImGui::Render(); // 렌더링할 것들 기록 끝
}

void ImGuiHandler::UpdateGUI()
{
    // ImGui::Checkbox("Use Texture", &m_BasicPixelConstantBufferData.useTexture);
    ImGui::Checkbox("Wireframe", &RenderSettings::Get().drawAsWire);
    ImGui::Checkbox("NormalVector", &RenderSettings::Get().drawNormalVector);
    if (ImGui::CollapsingHeader("Blinn-Phong"))
    {
        ImGui::SliderFloat("ks", &RenderSettings::Get().LightPhong.ks, 0.0f, 1.0f);
        ImGui::SliderFloat("shininess", &RenderSettings::Get().LightPhong.shininess, 128.0f, 256.0f);
     
    }
    
    if (ImGui::CollapsingHeader("world"))
    {
        RenderWorldActors(world);
        RenderRendererInfo(renderer);
    }
    
    if (ImGui::CollapsingHeader("Import##ImportSection"))
    {
         // FBXImporter 패널
        {
            static char g_FbxPath[512] = "D:/Assets/Test.fbx";
            static bool g_ImportFlipUV = true;
            static bool g_ImportLeftHanded = true;

            // UX: 마지막 에러/상태 표시
            static std::string g_LastError;
            static std::string g_LastImported;

            // UX: 파일 다이얼로그 초기 폴더 기억
            static std::wstring g_LastDirW;

            // 1) 경로 입력 + Open 버튼(같은 줄)
            ImGui::TextUnformatted("FBX Path");
            ImGui::SameLine();
            ImGui::PushItemWidth(-80.0f); // 오른쪽에 버튼 공간 남김
            ImGui::InputText("##FbxPath", g_FbxPath, IM_ARRAYSIZE(g_FbxPath));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // 폴더 아이콘 대신 우선 "..." (원하면 FontAwesome로 바꿔줄게)
            if (ImGui::Button("..."))
            {
                
                HWND ownerHwnd = AppBase::Get().GetMainWindow()->GetHwnd(); 
                std::wstring selected;
                if (OpenFileDialog_FBX(ownerHwnd, selected, g_LastDirW))
                {
                    // last dir 업데이트
                    const size_t pos = selected.find_last_of(L"\\/");
                    if (pos != std::wstring::npos)
                        g_LastDirW = selected.substr(0, pos);

                    std::string utf8 = WideToUtf8(selected);
                    strncpy_s(g_FbxPath, utf8.c_str(), _TRUNCATE);

                    g_LastError.clear();
                }
                
            }

            // 2) 옵션
            ImGui::Checkbox("Flip UV", &g_ImportFlipUV);
            ImGui::Checkbox("Convert To Left-Handed", &g_ImportLeftHanded);

            // 3) Import 버튼 비활성 조건 (빈 문자열 등)
            const bool hasPath = g_FbxPath[0] != '\0';
            if (!hasPath)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Import##ImportButton"))
            {
                g_LastError.clear();

                
                ImportOptions opt;
                opt.bFlipUV = g_ImportFlipUV;
                opt.bConvertToLeftHanded = g_ImportLeftHanded;

                ImportResult result;
                FbxImporter Importer;
                if (FbxImporter::ImportFBX(g_FbxPath, opt, result))
                {
                    if (const StaticMeshActor* actor = world->SpawnActor<StaticMeshActor>())
                    {
                        if (actor->GetRootComponent())
                        {
                            actor->GetRootComponent()->GetRelativeTransform().SetPosition(0.0f, 0.0f, -1.5f);
                            actor->GetRootComponent()->GetRelativeTransform().SetScale(0.02f, 0.02f, 0.02f);
                            
                        }
                        if (StaticMeshComponent* smc = actor->GetMeshComponent())
                        {
                            smc->SetMesh(result.Mesh);
                            smc->SetMaterial(result.Materials);
                        }
                    }
                    g_LastImported = g_FbxPath;
                }
                else
                {
                    g_LastError = "Import failed. Check file path / format / importer logs.";
                }
            }

            if (!hasPath)
            {
                ImGui::EndDisabled();
            }

            // 4) 상태 표시
            if (!g_LastImported.empty())
            {
                ImGui::Separator();
                ImGui::Text("Last Imported: %s", g_LastImported.c_str());
            }

            if (!g_LastError.empty())
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_LastError.c_str());
            }
        }
        ImGui::Separator();
    }
}

void ImGuiHandler::Render()
{
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // GUI 렌더링
}

void ImGuiHandler::RenderWorldActors(World* world)
{
    if (world)
    {
        for (std::shared_ptr<Actor> actor : world->GetActors())
        {
            Actor* actorPtr = actor.get();
            if (dynamic_cast<DirectionalLightActor*>(actorPtr))
            {
                if (SceneComponent* sceneComponent = actor->GetRootComponent())
                {
                    Transform& transform = sceneComponent->GetRelativeTransform();
                    ImGui::SliderFloat3("LightRotation", &transform.m_Rot.Pitch, -3.14f, 3.14f);
                }
            }
        }
    }
}

void ImGuiHandler::RenderRendererInfo(Renderer* renderer)
{
    if (renderer)
    {
        const ShadowOutput& Shadow = renderer->GetShadowOutput();
        for (const ShadowMap& dirMap : Shadow.directionalMaps)
        {
            ImGui::Image((ImTextureID)dirMap.GetSRV(), ImVec2 (400, 400));
        }
    }
}

