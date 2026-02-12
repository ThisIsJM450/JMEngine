#include "ImGuiHandler.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shobjidl.h>

#include "../Core/AppBase.h"
#include "../Core/Win32Window.h"
#include "../Graphics/Dx11Context.h"
#include "../Core/Settings/RenderSettings.h"
#include "../Editor/EditorContext.h"
#include "../Game/World.h"
#include "../Game/Actors/DirectionalLightActor.h"
#include "../Game/Actors/SpotLightActor.h"
#include "../Game/Actors/StaticMeshActor.h"
#include "../Game/Animation/AnimInstance.h"
#include "../Game/Characters/Character.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshData.h"
#include "../Graphics/Importer/FBXImporter.h"
#include "../Graphics/Importer/FBXImporter_Animation.h"
#include "../Renderer/Renderer.h"

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
            { L"GLTF Files (*.gltf)", L"*.gltf" },
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
    io.DisplaySize = ScreenBox;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // 도킹
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // 윈도우화
    ImGui::StyleColorsDark();
    SetupImGuiStyle_Minimal();
    SetupImGuiColors_Dark();

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
    // ImGui::Begin("Scene Control");
    //
    // // ImGui가 측정해주는 Framerate 출력
    // ImGui::Text("Average %.3f ms/frame (%.1f FPS)",
    //             1000.0f / ImGui::GetIO().Framerate,
    //             ImGui::GetIO().Framerate);

    // UpdateGUI(); // 추가적으로 사용할 GUI
    UpdateTabs();

   // ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));

    //m_guiWidth = int(ImGui::GetWindowWidth());

    //ImGui::End();
    ImGui::Render(); // 렌더링할 것들 기록 끝
}

// void ImGuiHandler::UpdateGUI()
// {
//     // ImGui::Checkbox("Use Texture", &m_BasicPixelConstantBufferData.useTexture);
//     ImGui::Checkbox("Wireframe", &RenderSettings::Get().drawAsWire);
//     ImGui::Checkbox("NormalVector", &RenderSettings::Get().drawNormalVector);
//     if (ImGui::CollapsingHeader("Blinn-Phong"))
//     {
//         ImGui::SliderFloat("ks", &RenderSettings::Get().LightPhong.ks, 0.0f, 1.0f);
//         ImGui::SliderFloat("shininess", &RenderSettings::Get().LightPhong.shininess, 128.0f, 256.0f);
//      
//     }
//     
//     // if (ImGui::CollapsingHeader("world"))
//     // {
//     //     //RenderWorldActors(world);
//     //     //(renderer);
//     // }
//     
//     if (ImGui::CollapsingHeader("Import##ImportSection"))
//     {
//          // FBXImporter 패널
//         {
//             static char g_FbxPath[512] = "D:/Assets/Test.fbx";
//             static bool g_ImportFlipUV = true;
//             static bool g_ImportLeftHanded = true;
//
//             // UX: 마지막 에러/상태 표시
//             static std::string g_LastError;
//             static std::string g_LastImported;
//
//             // UX: 파일 다이얼로그 초기 폴더 기억
//             static std::wstring g_LastDirW;
//
//             // 1) 경로 입력 + Open 버튼(같은 줄)
//             ImGui::TextUnformatted("FBX Path");
//             ImGui::SameLine();
//             ImGui::PushItemWidth(-80.0f); // 오른쪽에 버튼 공간 남김
//             ImGui::InputText("##FbxPath", g_FbxPath, IM_ARRAYSIZE(g_FbxPath));
//             ImGui::PopItemWidth();
//
//             ImGui::SameLine();
//
//             // 폴더 아이콘 대신 우선 "..." (원하면 FontAwesome로 바꿔줄게)
//             if (ImGui::Button("..."))
//             {
//                 
//                 HWND ownerHwnd = AppBase::Get().GetMainWindow()->GetHwnd(); 
//                 std::wstring selected;
//                 if (OpenFileDialog_FBX(ownerHwnd, selected, g_LastDirW))
//                 {
//                     // last dir 업데이트
//                     const size_t pos = selected.find_last_of(L"\\/");
//                     if (pos != std::wstring::npos)
//                         g_LastDirW = selected.substr(0, pos);
//
//                     std::string utf8 = WideToUtf8(selected);
//                     strncpy_s(g_FbxPath, utf8.c_str(), _TRUNCATE);
//
//                     g_LastError.clear();
//                 }
//                 
//             }
//
//             // 2) 옵션
//             ImGui::Checkbox("Flip UV", &g_ImportFlipUV);
//             ImGui::Checkbox("Convert To Left-Handed", &g_ImportLeftHanded);
//
//             // 3) Import 버튼 비활성 조건 (빈 문자열 등)
//             const bool hasPath = g_FbxPath[0] != '\0';
//             if (!hasPath)
//             {
//                 ImGui::BeginDisabled();
//             }
//
//             if (ImGui::Button("Import##ImportButton"))
//             {
//                 g_LastError.clear();
//
//                 
//                 ImportOptions opt;
//                 opt.bFlipUV = g_ImportFlipUV;
//                 opt.bConvertToLeftHanded = g_ImportLeftHanded;
//
//                 ImportResult result;
//                 FbxImporter Importer;
//                 if (FbxImporter::ImportFBX(g_FbxPath, opt, result))
//                 {
//                     if (const StaticMeshActor* actor = GEditor.world->SpawnActor<StaticMeshActor>(std::string("FBX")))
//                     {
//                         if (actor->GetRootComponent())
//                         {
//                             actor->GetRootComponent()->GetRelativeTransform().SetPosition(0.0f, 0.0f, -2.5f);
//                             actor->GetRootComponent()->GetRelativeTransform().SetScale(0.02f, 0.02f, 0.02f);
//                         }
//                         if (StaticMeshComponent* smc = actor->GetMeshComponent())
//                         {
//                             smc->SetMesh(result.Mesh);
//                             smc->SetMaterial(result.Materials);
//                         }
//                     }
//                     g_LastImported = g_FbxPath;
//                 }
//                 else
//                 {
//                     g_LastError = "Import failed. Check file path / format / importer logs.";
//                 }
//             }
//
//             if (!hasPath)
//             {
//                 ImGui::EndDisabled();
//             }
//
//             // 4) 상태 표시
//             if (!g_LastImported.empty())
//             {
//                 ImGui::Separator();
//                 ImGui::Text("Last Imported: %s", g_LastImported.c_str());
//             }
//
//             if (!g_LastError.empty())
//             {
//                 ImGui::Separator();
//                 ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_LastError.c_str());
//             }
//         }
//         ImGui::Separator();
//     }
// }

void ImGuiHandler::UpdateTabs()
{
    // 0) 메인 도킹 창(전체 화면)
    ImGuiWindowFlags window_flags =
        //ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##MainDockspace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // 도킹 스페이스
    ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    // ToolBar
    /*
    if (ImGui::BeginMenuBar())
    {

        if (ImGui::BeginMenu("Window"))
        {
            // 패널 토글용 bool들 연결 가능
            ImGui::EndMenu();
        }
   
        ImGui::EndMenuBar();
    }
    */

    // 1) 개별 패널 렌더 (각각 Begin/End)
    RenderViewportPanel();
    RenderOutlinerPanel();
    RenderDetailsPanel();
    RenderContentBrowserPanel();
   // RenderImportPanel(); // 너의 FBX import도 여기로 분리 추천
    
    ImGui::End();
}

void ImGuiHandler::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
//
// void ImGuiHandler::RenderWorldActors(World* world)
// {
//     if (world)
//     {
//         for (std::shared_ptr<Actor> actor : world->GetActors())
//         {
//             Actor* actorPtr = actor.get();
//             if (dynamic_cast<DirectionalLightActor*>(actorPtr))
//             {
//                 if (SceneComponent* sceneComponent = actor->GetRootComponent())
//                 {
//                     Transform& transform = sceneComponent->GetRelativeTransform();
//                     ImGui::SliderFloat3("LightRotation", &transform.m_Rot.Pitch, -3.14f, 3.14f);
//                 }
//             }
//             if (dynamic_cast<SpotLightActor*>(actorPtr))
//             {
//                 if (SceneComponent* sceneComponent = actor->GetRootComponent())
//                 {
//                     Transform& transform = sceneComponent->GetRelativeTransform();
//                     ImGui::SliderFloat3("SpotLightLocation", &transform.m_Pos.x, -4.f, 4.f);
//                 }
//             }
//         }
//     }
// }

// void ImGuiHandler::RenderRendererInfo(Renderer* renderer)
// {
//     if (renderer)
//     {
//         const ShadowOutput& Shadow = renderer->GetShadowOutput();
//         for (const ShadowMap& dirMap : Shadow.spotMaps)
//         {
//             ImGui::Image((ImTextureID)dirMap.GetSRV(), ImVec2 (400, 400));
//         }
//     }
// }

void ImGuiHandler::RenderViewportPanel()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("Viewport", nullptr, flags);
    RenderViewportContents();
    ImGui::End();
}

void ImGuiHandler::RenderViewportContents()
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 2.0f || avail.y < 2.0f)
    {
        return;
    }

    ID3D11ShaderResourceView* srv = Dx11Context::Get().GetViewBufferSRV();
    if (!srv)
    {
        return;
    }

    // --- 텍스처 원본 크기 조회(가능하면 Dx11Context에 저장해둔 값 쓰는 게 더 좋음) ---
    int texW = 0, texH = 0;
    {
        ID3D11Resource* res = nullptr;
        srv->GetResource(&res);
        if (res)
        {
            ID3D11Texture2D* t2d = nullptr;
            if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&t2d)) && t2d)
            {
                D3D11_TEXTURE2D_DESC d{};
                t2d->GetDesc(&d);
                texW = (int)d.Width;
                texH = (int)d.Height;
                t2d->Release();
            }
            res->Release();
        }
    }
    if (texW <= 0 || texH <= 0)
    {
        return;
    }

    // --- 언리얼식: 패널 꽉 채우되 비율 유지(센터 크롭) ---
    const float texAspect  = (float)texW / (float)texH;
    const float viewAspect = avail.x / avail.y;

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);

    if (viewAspect > texAspect)
    {
        // 패널이 더 가로로 넓음 -> 위/아래 크롭
        const float neededTexH = (float)texW / viewAspect;
        float crop = ((float)texH - neededTexH) / (float)texH;
        if (crop < 0.0f) crop = 0.0f;
        uv0.y = crop * 0.5f;
        uv1.y = 1.0f - crop * 0.5f;
    }
    else
    {
        // 패널이 더 세로로 김 -> 좌/우 크롭
        const float neededTexW = (float)texH * viewAspect;
        float crop = ((float)texW - neededTexW) / (float)texW;
        if (crop < 0.0f) crop = 0.0f;
        uv0.x = crop * 0.5f;
        uv1.x = 1.0f - crop * 0.5f;
    }

    // 배경(검정) 깔기: 언리얼처럼 뷰포트 주변이 깔끔해짐
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p0, ImVec2(p0.x + avail.x, p0.y + avail.y), IM_COL32(0, 0, 0, 255));
    }

    // 실제 이미지 그리기(패널 크기 그대로, uv로 크롭)
    ImTextureID tex = (ImTextureID)srv;
    ImGui::Image(tex, avail, uv0, uv1);
    
    // 여기: Viewport에 드롭 받기
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ID"))
        {
            if (payload->DataSize == sizeof(AssetID))
            {
                const AssetID droppedId = *(const AssetID*)payload->Data;

                const AssetMeta* meta = GAssetRegistry->GetMeta(droppedId);
                if (meta)
                {
                    OnDropToViewport(droppedId, *meta);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // --- 클릭 시: "보이는 영역 기준" UV -> "원본 텍스처 UV"로 변환 ---
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 imgMin = ImGui::GetItemRectMin();
        const ImVec2 imgMax = ImGui::GetItemRectMax();
        const ImVec2 mouse  = ImGui::GetIO().MousePos;

        const float localX = (mouse.x - imgMin.x) / (imgMax.x - imgMin.x); // 0~1
        const float localY = (mouse.y - imgMin.y) / (imgMax.y - imgMin.y); // 0~1

        // 안전 클램프
        float uView = localX; if (uView < 0.0f) uView = 0.0f; if (uView > 1.0f) uView = 1.0f;
        float vView = localY; if (vView < 0.0f) vView = 0.0f; if (vView > 1.0f) vView = 1.0f;

        // 크롭된 uv로 매핑 (이게 "원본 텍스처 uv")
        const float uTex = uv0.x + uView * (uv1.x - uv0.x);
        const float vTex = uv0.y + vView * (uv1.y - uv0.y);

        // 여기 uTex/vTex로 ray pick (원하면 NDC로 바꿔도 됨)
        std::weak_ptr<Actor> hitActor; // world->PickActorByRay(uTex, vTex, renderer->GetCamera());
        GEditor.selection.SelectedActor = hitActor;
    }
}


void ImGuiHandler::RenderOutlinerPanel()
{
    ImGui::Begin("Outliner");
    RenderOutlinerPanelContents();
    ImGui::End();
}

void ImGuiHandler::RenderOutlinerPanelContents()
{
    static char filter[128] = "";
    ImGui::InputTextWithHint("##filter", "Search...", filter, IM_ARRAYSIZE(filter));
    ImGui::Separator();

    if (!GEditor.world)
    {
        ImGui::TextUnformatted("World is null.");
        return;
    }
    
    const auto& actors = GEditor.world->GetActors(); // 가정

    for (std::shared_ptr<Actor> a : actors)
    {
        if (!a) continue;
        std::weak_ptr<Actor> WeakActor = a;

        const std::string nameStr = WeakActor.lock()->GetName() + std::string("##") + std::to_string(WeakActor.lock()->GetUniqueId()); 
        const char* name = nameStr.c_str();
        

        if (filter[0] != '\0' && strstr(name, filter) == nullptr)
            continue;

        const bool selected = GEditor.selection.SelectedActor.expired() == false ? GEditor.selection.SelectedActor.lock() == WeakActor.lock() : false;
        if (ImGui::Selectable(name, selected))
        {
            GEditor.selection.SelectedActor = WeakActor;
        }
    }
}

static bool DrawTransformUI(Transform& t) // 너 Transform API에 맞게 수정
{
    bool changed = false;

    // 아래는 예시: 네 Transform이 Vector3를 어떻게 노출하는지에 맞게 수정
    DirectX::XMFLOAT3& pos = t.m_Pos;
    Rotation& rot = t.m_Rot;
    DirectX::XMFLOAT3& scl = t.m_Scale;

    ImGui::DragFloat3("Position", &pos.x, 0.01f);
    ImGui::DragFloat3("Rotation", &rot.Pitch, 0.1f);
    ImGui::DragFloat3("Scale",    &scl.x, 0.01f);

    return changed;
}

void ImGuiHandler::RenderDetailsPanel()
{
    ImGui::Begin("Details");
    RenderDetailsPanelContents();
    ImGui::End();
}

void ImGuiHandler::RenderDetailsPanelContents()
{
    std::weak_ptr<Actor> a = GEditor.selection.SelectedActor;
    if (a.expired())
    {
        ImGui::TextUnformatted("No selection.");
        return;
    }

    ImGui::Text("Actor: %s", a.lock()->GetName().c_str());
    ImGui::Separator();

    // Transform 
    if (SceneComponent* sceneComponent = a.lock()->GetRootComponent())
    {
        Transform& transform = sceneComponent->GetRelativeTransform();
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawTransformUI(transform);
        }
    }

    // Components(리플렉션 없으니 타입별 수동 렌더로 시작)
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const std::vector<std::shared_ptr<ActorComponent>>& comps = a.lock()->GetComponents(); // 가정: vector<Component*>
        for (std::shared_ptr<ActorComponent> c : comps)
        {
            if (!c) continue;

            std::string header = c->GetTypeName(); // 가정
            header += "##";
            header += c->GetName();

            if (ImGui::CollapsingHeader(header.c_str()))
            {
                RenderComponentDetails(c.get()); // 아래에서 구현
            }
        }
    }
}

void ImGuiHandler::RenderComponentDetails(ActorComponent* component)
{
    if (SkeletalMeshComponent* SKComp = dynamic_cast<SkeletalMeshComponent*>(component))
    {
        if (Skeleton* Skeleton = SKComp->GetSkeleton())
        {
            if (AnimInstance* AnimInst = SKComp->GetAnimInstance())
            {
                std::string displayName = "AnimSequence";
                ImGui::Selectable(displayName.c_str());

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ID"))
                    {
                        if (payload->DataSize == sizeof(AssetID))
                        {
                            const AssetID droppedId = *(const AssetID*)payload->Data;

                            const AssetMeta* meta = GAssetRegistry->GetMeta(droppedId);
                            if (meta && meta->type == AssetType::Animation)
                            {
                                std::shared_ptr<AnimSequenceAsset> AS_Asset = FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(meta->sourcePath, *Skeleton);
                                AnimInst->SetSequence(AS_Asset);
                                AnimInst->Play();
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }
    }
}

void ImGuiHandler::RenderContentBrowserPanel()
{
    RenderContentBrowserContents();
}

void ImGuiHandler::RenderContentBrowserContents()
{
    if (GAssetRegistry)
    {
        ContentBrowserState CBState;
        m_contentBrowserPanel.RenderContentBrowserPanel(*const_cast<AssetRegistry*>(GAssetRegistry), CBState);
    }
}

void ImGuiHandler::SetupImGuiStyle_Minimal()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // 전체 창 외곽 여백(윈도우 안쪽 패딩)
    s.WindowPadding = ImVec2(0.0f, 0.0f);

    // 창 최소 크기(원하면)
    // s.WindowMinSize = ImVec2(100.0f, 100.0f);

    // 프레임(버튼/체크박스 등) 안쪽 패딩
    s.FramePadding = ImVec2(4.0f, 3.0f);

    // 아이템 간격(위젯 사이 간격)
    s.ItemSpacing = ImVec2(6.0f, 4.0f);

    // 섹션 간격
    s.ItemInnerSpacing = ImVec2(6.0f, 4.0f);

    // 컬럼/테이블 패딩
    s.CellPadding = ImVec2(4.0f, 2.0f);

    // 인덴트(트리 들여쓰기)
    s.IndentSpacing = 12.0f;

    // 스크롤바 크기
    s.ScrollbarSize = 12.0f;

    // 창 둥글기/보더
    s.WindowRounding = 0.0f;
    s.FrameRounding  = 0.0f;
    s.GrabRounding   = 0.0f;
    s.TabRounding    = 0.0f;

    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;

    // 도킹 분할선/스플리터 두께(있으면 더 “언리얼”처럼 얇게)
    s.DockingSeparatorSize = 1.0f;
}

void ImGuiHandler::SetupImGuiColors_Dark()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // 기본 배경
    c[ImGuiCol_WindowBg]        = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

    // 프레임(버튼/체크박스 배경)
    c[ImGuiCol_FrameBg]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

    // 타이틀바(안 쓰면 NoTitleBar로 숨기면 됨)
    c[ImGuiCol_TitleBg]         = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // 탭(도킹)
    c[ImGuiCol_Tab]             = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_TabHovered]      = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_TabActive]       = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_TabUnfocused]    = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]=ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    // 헤더(트리/셀렉터)
    c[ImGuiCol_Header]          = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    // 스크롤바
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    // 구분선
    c[ImGuiCol_Separator]       = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_SeparatorHovered]= ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
}

void ImGuiHandler::OnDropToViewport(AssetID id, const AssetMeta& meta)
{
    if (meta.type == AssetType::StaticMesh || meta.type == AssetType::SkeletalMesh)
    {
        ImportOptions opt;
        ImportResult result;
        if (FbxImporter::ImportFBX(meta.sourcePath, opt, result))
        {
            if (result.Type == EImportedMeshType::Static)
            {
                if (const StaticMeshActor* actor = GEditor.world->SpawnActor<StaticMeshActor>(std::string("FBX")))
                {
                    if (actor->GetRootComponent())
                    {
                        actor->GetRootComponent()->GetRelativeTransform().SetPosition(0.0f, 0.0f, -2.5f);
                        actor->GetRootComponent()->GetRelativeTransform().SetScale(0.02f, 0.02f, 0.02f);
                    }
                    if (StaticMeshComponent* smc = actor->GetMeshComponent())
                    {
                        smc->SetMesh(result.StaticMesh);
                        smc->SetMaterial(result.Materials);
                    }
                }
            }
            if (result.Type == EImportedMeshType::Skeletal)
            {
                if (const Character* actor = GEditor.world->SpawnActor<Character>(std::string("Character")))
                {
                    if (actor->GetRootComponent())
                    {
                        actor->GetRootComponent()->GetRelativeTransform().SetPosition(0.0f, 0.0f, -2.5f);
                        actor->GetRootComponent()->GetRelativeTransform().SetScale(0.02f, 0.02f, 0.02f);
                    }
                    if (SkeletalMeshComponent* skc = actor->GetSkeletalComponent())
                    {
                        skc->SetMesh(result.SkeletalMesh);
                        skc->SetMaterial(result.Materials);
                    }
                }
            }
        }
    }
}

