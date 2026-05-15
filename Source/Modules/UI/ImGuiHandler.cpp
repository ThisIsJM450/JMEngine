#include "ImGuiHandler.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shobjidl.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <ctime>
#include <DirectXCollision.h>

#include "../Core/AppBase.h"
#include "../Core/Win32Window.h"
#include "../Graphics/Dx11Context.h"
#include "../Core/Settings/RenderSettings.h"
#include "../Editor/EditorContext.h"
#include "../Editor/ComponentReflection.h"
#include "../Game/World.h"
#include "../Game/Actors/DirectionalLightActor.h"
#include "../Game/Actors/SpotLightActor.h"
#include "../Game/Actors/StaticMeshActor.h"
#include "../Game/Animation/AnimInstance.h"
#include "../Game/Animation/AnimSequence.h"
#include "../Game/Animation/FootIKSample.h"
#include "../Game/Characters/Character.h"
#include "../Game/Components/CharacterMovementComponenet.h"
#include "../Game/Components/MeshComponent.h"
#include "../Game/Components/SceneComponent.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshData.h"
#include "../Graphics/Importer/FBXImporter.h"
#include "../Graphics/Importer/FBXImporter_Animation.h"
#include "../Renderer/Renderer.h"
#include "../Scene/Level/LevelAsset.h"
#include "../Scene/Level/LevelLoader.h"
#include "../Scene/Level/LevelSaver.h"
#include "../Test/RootMotionTestEnvironment.h"

namespace 
{
    static constexpr const char* kRuntimeValidationLogPath =
        "D:\\Projects\\JMEngine\\JMEngine\\Source\\RootMotionRuntimeValidation.log";
    static constexpr const char* kRuntimeValidationCsvPath =
        "D:\\Projects\\JMEngine\\JMEngine\\Source\\RootMotionRuntimeValidation_FrameBone.csv";

    static std::string GetLocalTimestampString()
    {
        std::time_t now = std::time(nullptr);
        std::tm tmNow{};
        localtime_s(&tmNow, &now);
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmNow);
        return std::string(buf);
    }

    static void AppendRuntimeValidationLogLine(const std::string& line)
    {
        std::ofstream logFile(kRuntimeValidationLogPath, std::ios::out | std::ios::app);
        if (!logFile.is_open())
        {
            return;
        }

        logFile << "[" << GetLocalTimestampString() << "] " << line << "\n";
    }

    static std::string ToLowerASCII(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return (char)std::tolower(c);
        });
        return s;
    }

    static bool ContainsCaseInsensitive(const std::string& text, const char* pattern)
    {
        if (!pattern || pattern[0] == '\0')
        {
            return true;
        }

        const std::string lowerText = ToLowerASCII(text);
        const std::string lowerPattern = ToLowerASCII(std::string(pattern));
        return lowerText.find(lowerPattern) != std::string::npos;
    }

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

    enum class ViewportGizmoMode
    {
        Translate = 0,
        Rotate = 1,
        Scale = 2
    };

    static ViewportGizmoMode g_ViewportGizmoMode = ViewportGizmoMode::Translate;
    static bool g_ShowFootIKDebugOverlay = false;

    static const char* GizmoModeToString(const ViewportGizmoMode mode)
    {
        switch (mode)
        {
        case ViewportGizmoMode::Translate: return "Translate";
        case ViewportGizmoMode::Rotate:    return "Rotate";
        case ViewportGizmoMode::Scale:     return "Scale";
        default:                           return "Unknown";
        }
    }

    static float DistancePointToSegmentSq(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float apx = p.x - a.x;
        const float apy = p.y - a.y;
        const float lenSq = abx * abx + aby * aby;
        if (lenSq <= 1e-6f)
        {
            const float dx = p.x - a.x;
            const float dy = p.y - a.y;
            return dx * dx + dy * dy;
        }

        float t = (apx * abx + apy * aby) / lenSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        const float qx = a.x + abx * t;
        const float qy = a.y + aby * t;
        const float dx = p.x - qx;
        const float dy = p.y - qy;
        return dx * dx + dy * dy;
    }

    static bool BuildPickingRayFromTextureUV(
        const Scene& scene,
        const float uTex,
        const float vTex,
        DirectX::XMVECTOR& outOrigin,
        DirectX::XMVECTOR& outDir)
    {
        using namespace DirectX;
        const float ndcX = uTex * 2.0f - 1.0f;
        const float ndcY = 1.0f - vTex * 2.0f;

        const ViewInfo& viewInfo = scene.GetViewInfo();
        const XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewInfo.view * viewInfo.proj);
        const XMVECTOR nearNdc = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
        const XMVECTOR farNdc = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);

        const XMVECTOR nearWorld = XMVector3TransformCoord(nearNdc, invViewProj);
        const XMVECTOR farWorld = XMVector3TransformCoord(farNdc, invViewProj);
        const XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(farWorld, nearWorld));
        if (XMVectorGetX(XMVector3LengthSq(dir)) <= 1e-10f)
        {
            return false;
        }

        outOrigin = nearWorld;
        outDir = dir;
        return true;
    }

    static bool IntersectRayBoundBox(
        DirectX::FXMVECTOR rayOrigin,
        DirectX::FXMVECTOR rayDir,
        const BoundBox& b,
        float& outDistance)
    {
        using namespace DirectX;
        const BoundingBox box(
            XMFLOAT3(b.Center.x, b.Center.y, b.Center.z),
            XMFLOAT3(b.Extents.x, b.Extents.y, b.Extents.z));
        return box.Intersects(rayOrigin, rayDir, outDistance);
    }

    static bool IntersectRayPlane(
        DirectX::FXMVECTOR rayOrigin,
        DirectX::FXMVECTOR rayDir,
        DirectX::FXMVECTOR planePoint,
        DirectX::FXMVECTOR planeNormal,
        DirectX::XMVECTOR& outPoint)
    {
        using namespace DirectX;
        const float denom = XMVectorGetX(XMVector3Dot(rayDir, planeNormal));
        if (std::fabs(denom) <= 1e-6f)
        {
            return false;
        }

        const float t = XMVectorGetX(XMVector3Dot(XMVectorSubtract(planePoint, rayOrigin), planeNormal)) / denom;
        if (t < 0.0f)
        {
            return false;
        }

        outPoint = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, t));
        return true;
    }

    static void ScreenPosToTextureUV(
        const ImVec2& screenPos,
        const ImVec2& imageMin,
        const ImVec2& imageMax,
        const ImVec2& uv0,
        const ImVec2& uv1,
        float& outUTex,
        float& outVTex)
    {
        float localX = (screenPos.x - imageMin.x) / (imageMax.x - imageMin.x);
        float localY = (screenPos.y - imageMin.y) / (imageMax.y - imageMin.y);
        if (localX < 0.0f) localX = 0.0f;
        if (localX > 1.0f) localX = 1.0f;
        if (localY < 0.0f) localY = 0.0f;
        if (localY > 1.0f) localY = 1.0f;

        outUTex = uv0.x + localX * (uv1.x - uv0.x);
        outVTex = uv0.y + localY * (uv1.y - uv0.y);
    }

    static bool ComputeActorWorldBounds(const Actor& actor, BoundBox& outBounds)
    {
        bool hasAny = false;
        DirectX::XMFLOAT3 minP(FLT_MAX, FLT_MAX, FLT_MAX);
        DirectX::XMFLOAT3 maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        const std::vector<std::shared_ptr<ActorComponent>>& comps = actor.GetComponents();
        for (const std::shared_ptr<ActorComponent>& c : comps)
        {
            MeshComponent* mc = dynamic_cast<MeshComponent*>(c.get());
            if (!mc)
            {
                continue;
            }

            SceneProxyBase* proxy = mc->GetProxy();
            if (!proxy || proxy->IsCubeMap())
            {
                continue;
            }

            const BoundBox b = proxy->GetBounds();
            const DirectX::XMFLOAT3 bMin(
                b.Center.x - b.Extents.x,
                b.Center.y - b.Extents.y,
                b.Center.z - b.Extents.z);
            const DirectX::XMFLOAT3 bMax(
                b.Center.x + b.Extents.x,
                b.Center.y + b.Extents.y,
                b.Center.z + b.Extents.z);

            minP.x = (std::min)(minP.x, bMin.x);
            minP.y = (std::min)(minP.y, bMin.y);
            minP.z = (std::min)(minP.z, bMin.z);
            maxP.x = (std::max)(maxP.x, bMax.x);
            maxP.y = (std::max)(maxP.y, bMax.y);
            maxP.z = (std::max)(maxP.z, bMax.z);
            hasAny = true;
        }

        if (!hasAny)
        {
            const SceneComponent* root = actor.GetRootComponent();
            if (!root)
            {
                return false;
            }

            const DirectX::XMFLOAT3 p = root->GetWorldLocation();
            outBounds.Center = p;
            outBounds.Extents = DirectX::XMFLOAT3(0.25f, 0.25f, 0.25f);
            return true;
        }

        outBounds.Center = DirectX::XMFLOAT3(
            (minP.x + maxP.x) * 0.5f,
            (minP.y + maxP.y) * 0.5f,
            (minP.z + maxP.z) * 0.5f);
        outBounds.Extents = DirectX::XMFLOAT3(
            (maxP.x - minP.x) * 0.5f,
            (maxP.y - minP.y) * 0.5f,
            (maxP.z - minP.z) * 0.5f);
        return true;
    }

    static std::weak_ptr<Actor> PickActorByRay(World* world, DirectX::FXMVECTOR rayOrigin, DirectX::FXMVECTOR rayDir)
    {
        std::weak_ptr<Actor> picked;
        if (!world)
        {
            return picked;
        }

        float bestDistance = FLT_MAX;
        for (const std::shared_ptr<Actor>& a : world->GetActors())
        {
            if (!a)
            {
                continue;
            }

            BoundBox bounds{};
            if (!ComputeActorWorldBounds(*a, bounds))
            {
                continue;
            }

            float hitDistance = 0.0f;
            if (IntersectRayBoundBox(rayOrigin, rayDir, bounds, hitDistance) && hitDistance >= 0.0f && hitDistance < bestDistance)
            {
                bestDistance = hitDistance;
                picked = a;
            }
        }

        return picked;
    }

    static bool ProjectWorldPointToViewport(
        const Scene& scene,
        const DirectX::XMFLOAT3& world,
        const ImVec2& imageMin,
        const ImVec2& imageMax,
        const ImVec2& uv0,
        const ImVec2& uv1,
        ImVec2& outScreen,
        float* outDepth01 = nullptr)
    {
        using namespace DirectX;
        const ViewInfo& viewInfo = scene.GetViewInfo();
        const XMMATRIX vp = viewInfo.view * viewInfo.proj;
        const XMVECTOR p = XMVectorSet(world.x, world.y, world.z, 1.0f);
        const XMVECTOR clip = XMVector4Transform(p, vp);
        const float w = XMVectorGetW(clip);
        if (std::fabs(w) <= 1e-6f)
        {
            return false;
        }

        const XMVECTOR ndc = XMVectorDivide(clip, XMVectorReplicate(w));
        const float ndcX = XMVectorGetX(ndc);
        const float ndcY = XMVectorGetY(ndc);
        const float ndcZ = XMVectorGetZ(ndc);

        const float uTex = ndcX * 0.5f + 0.5f;
        const float vTex = (1.0f - ndcY) * 0.5f;

        const float denomX = uv1.x - uv0.x;
        const float denomY = uv1.y - uv0.y;
        if (std::fabs(denomX) <= 1e-6f || std::fabs(denomY) <= 1e-6f)
        {
            return false;
        }

        const float sx = (uTex - uv0.x) / denomX;
        const float sy = (vTex - uv0.y) / denomY;
        outScreen.x = imageMin.x + sx * (imageMax.x - imageMin.x);
        outScreen.y = imageMin.y + sy * (imageMax.y - imageMin.y);

        if (outDepth01)
        {
            *outDepth01 = ndcZ;
        }
        return true;
    }

    static bool SetDialogInitialFolder(IFileDialog* dialog, const std::wstring& initialDir)
    {
        if (!dialog || initialDir.empty())
        {
            return false;
        }

        IShellItem* folderItem = nullptr;
        const HRESULT hr = SHCreateItemFromParsingName(initialDir.c_str(), nullptr, IID_PPV_ARGS(&folderItem));
        if (FAILED(hr) || !folderItem)
        {
            return false;
        }

        dialog->SetFolder(folderItem);
        folderItem->Release();
        return true;
    }

    static bool OpenFileDialog_FBX(HWND ownerHwnd, std::wstring& outPath, const std::wstring& initialDir = L"")
    {
        outPath.clear();

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool didInit = SUCCEEDED(hrInit);

        IFileOpenDialog* pDlg = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
        if (FAILED(hr) || !pDlg)
        {
            if (didInit) CoUninitialize();
            return false;
        }

        COMDLG_FILTERSPEC rgSpec[] =
        {
            { L"FBX Files (*.fbx)", L"*.fbx" },
            { L"GLTF Files (*.gltf)", L"*.gltf" },
            { L"All Files (*.*)",   L"*.*"   }
        };
        pDlg->SetFileTypes(_countof(rgSpec), rgSpec);
        pDlg->SetFileTypeIndex(1);
        pDlg->SetTitle(L"Select FBX file");
        SetDialogInitialFolder(pDlg, initialDir);

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

    static bool OpenLevelDialog(HWND ownerHwnd, std::wstring& outPath, const std::wstring& initialDir = L"")
    {
        outPath.clear();

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool didInit = SUCCEEDED(hrInit);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
        {
            if (didInit) CoUninitialize();
            return false;
        }

        COMDLG_FILTERSPEC filters[] =
        {
            { L"Level Files (*.json)", L"*.json" },
            { L"All Files (*.*)", L"*.*" }
        };
        dialog->SetFileTypes(_countof(filters), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetTitle(L"Open Level");
        SetDialogInitialFolder(dialog, initialDir);

        hr = dialog->Show(ownerHwnd);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item)
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                {
                    outPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (didInit) CoUninitialize();
        return !outPath.empty();
    }

    static bool SaveLevelDialog(HWND ownerHwnd, std::wstring& outPath, const std::wstring& initialDir = L"", const std::wstring& defaultName = L"SavedLevel.json")
    {
        outPath.clear();

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool didInit = SUCCEEDED(hrInit);

        IFileSaveDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
        {
            if (didInit) CoUninitialize();
            return false;
        }

        COMDLG_FILTERSPEC filters[] =
        {
            { L"Level Files (*.json)", L"*.json" },
            { L"All Files (*.*)", L"*.*" }
        };
        dialog->SetFileTypes(_countof(filters), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetTitle(L"Save Level As");
        dialog->SetDefaultExtension(L"json");
        dialog->SetFileName(defaultName.c_str());
        SetDialogInitialFolder(dialog, initialDir);

        hr = dialog->Show(ownerHwnd);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item)
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                {
                    outPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (didInit) CoUninitialize();
        return !outPath.empty();
    }

    static void MarkLevelDirty(const std::string& reason = std::string())
    {
        GEditor.document.MarkDirty();
        if (!reason.empty())
        {
            GEditor.document.statusMessage = reason;
        }
    }

    static void SaveRecentLevelsToDisk();
    static std::string ToDisplayLevelPath(const std::string& path);

    static bool LoadEditorLevelFromPath(const std::string& path)
    {
        if (!GEditor.world)
        {
            GEditor.document.statusMessage = "Open failed: world is null.";
            return false;
        }

        LevelAsset level;
        if (!LevelAsset::LoadFromFile(path, level))
        {
            GEditor.document.statusMessage = "Open failed: could not parse level file.";
            return false;
        }

        GEditor.world->ClearActors();
        GEditor.selection.SelectedActor.reset();
        if (!LevelLoader::ApplyLevel(*GEditor.world, level))
        {
            GEditor.document.statusMessage = "Open failed: level apply returned false.";
            return false;
        }

        namespace fs = std::filesystem;
        const fs::path filePath(path);
        const std::string levelName = !level.name.empty() ? level.name : filePath.stem().string();
        GEditor.document.MarkSaved(path, levelName.empty() ? "SavedLevel" : levelName);
        SaveRecentLevelsToDisk();
        GEditor.document.statusMessage = std::string("Opened level: ") + ToDisplayLevelPath(path);
        return true;
    }

    static bool SaveEditorLevelToPath(const std::string& path)
    {
        if (!GEditor.world)
        {
            GEditor.document.statusMessage = "Save failed: world is null.";
            return false;
        }

        namespace fs = std::filesystem;
        const fs::path filePath(path);
        const std::string levelName = filePath.stem().string().empty() ? GEditor.document.currentLevelName : filePath.stem().string();
        if (!LevelSaver::SaveWorldToFile(*GEditor.world, path, levelName.empty() ? "SavedLevel" : levelName))
        {
            GEditor.document.statusMessage = "Save failed: could not write level file.";
            return false;
        }

        GEditor.document.MarkSaved(path, levelName.empty() ? "SavedLevel" : levelName);
        SaveRecentLevelsToDisk();
        GEditor.document.statusMessage = std::string("Saved level: ") + ToDisplayLevelPath(path);
        return true;
    }

    enum class PendingLevelActionType
    {
        None,
        NewLevel,
        OpenLevelPath,
        OpenLevelDialog
    };

    struct PendingLevelAction
    {
        PendingLevelActionType type = PendingLevelActionType::None;
        std::string path;
    };

    static PendingLevelAction g_PendingLevelAction;
    static bool g_ShowUnsavedLevelModal = false;
    static bool g_BypassUnsavedLevelCheckOnce = false;

    static std::filesystem::path GetProjectRootPath()
    {
        namespace fs = std::filesystem;
        fs::path current = fs::current_path();
        while (!current.empty())
        {
            if (fs::exists(current / "Contents") && fs::exists(current / "JMEngine.sln"))
            {
                return current;
            }
            if (!current.has_parent_path() || current.parent_path() == current)
            {
                break;
            }
            current = current.parent_path();
        }
        return fs::current_path();
    }

    static std::filesystem::path GetDefaultLevelsDirectoryPath()
    {
        namespace fs = std::filesystem;
        fs::path dir = GetProjectRootPath() / "Contents" / "Levels";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }

    static std::filesystem::path GetRecentLevelsStatePath()
    {
        return GetDefaultLevelsDirectoryPath() / ".recent_levels";
    }

    static std::string ToDisplayLevelPath(const std::string& path)
    {
        namespace fs = std::filesystem;
        if (path.empty())
        {
            return std::string();
        }

        std::error_code ec;
        const fs::path absolutePath = fs::weakly_canonical(fs::path(path), ec);
        const fs::path levelsRoot = fs::weakly_canonical(GetDefaultLevelsDirectoryPath(), ec);
        if (!ec && !absolutePath.empty() && !levelsRoot.empty())
        {
            const std::string abs = absolutePath.generic_string();
            const std::string root = levelsRoot.generic_string();
            if (abs.size() >= root.size() && abs.compare(0, root.size(), root) == 0)
            {
                std::string rel = abs.substr(root.size());
                while (!rel.empty() && rel.front() == '/')
                {
                    rel.erase(rel.begin());
                }
                return std::string("/Game/Levels/") + rel;
            }
        }

        return fs::path(path).generic_string();
    }

    static void SaveRecentLevelsToDisk()
    {
        namespace fs = std::filesystem;
        const fs::path statePath = GetRecentLevelsStatePath();
        std::error_code ec;
        fs::create_directories(statePath.parent_path(), ec);

        std::ofstream out(statePath, std::ios::out | std::ios::trunc);
        if (!out.is_open())
        {
            return;
        }

        for (const std::string& p : GEditor.document.recentLevelPaths)
        {
            out << p << "\n";
        }
    }

    static void LoadRecentLevelsFromDisk()
    {
        namespace fs = std::filesystem;
        GEditor.document.recentLevelPaths.clear();

        std::ifstream in(GetRecentLevelsStatePath());
        if (!in.is_open())
        {
            return;
        }

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::error_code ec;
            const fs::path normalized = fs::weakly_canonical(fs::path(line), ec);
            const std::string fixed = ec ? line : normalized.string();
            if (fs::exists(fixed))
            {
                GEditor.document.TouchRecentLevel(fixed);
            }
        }
    }

    static std::wstring GetPreferredLevelDirectoryW()
    {
        namespace fs = std::filesystem;
        if (!GEditor.document.currentLevelPath.empty())
        {
            const fs::path currentPath = fs::path(GEditor.document.currentLevelPath);
            if (currentPath.has_parent_path())
            {
                std::error_code ec;
                if (fs::exists(currentPath.parent_path(), ec))
                {
                    return currentPath.parent_path().wstring();
                }
            }
        }

        return GetDefaultLevelsDirectoryPath().wstring();
    }

    static std::wstring GetSuggestedLevelFilenameW()
    {
        namespace fs = std::filesystem;
        if (!GEditor.document.currentLevelPath.empty())
        {
            const fs::path currentPath = fs::path(GEditor.document.currentLevelPath);
            if (!currentPath.filename().empty())
            {
                return currentPath.filename().wstring();
            }
        }

        const std::string baseName = GEditor.document.currentLevelName.empty() ? std::string("NewLevel") : GEditor.document.currentLevelName;
        return Utf8ToWide(baseName + ".json");
    }

    static void CreateNewEditorLevel()
    {
        if (!GEditor.world)
        {
            GEditor.document.statusMessage = "New level failed: world is null.";
            return;
        }

        GEditor.world->ClearActors();
        GEditor.selection.SelectedActor.reset();
        GEditor.document.ResetToNewLevel("NewLevel");
        GEditor.document.statusMessage = "Created new empty level.";
    }

    static void QueuePendingLevelAction(PendingLevelActionType type, const std::string& path = std::string())
    {
        g_PendingLevelAction.type = type;
        g_PendingLevelAction.path = path;
        if (GEditor.document.bLevelDirty && !g_BypassUnsavedLevelCheckOnce)
        {
            g_ShowUnsavedLevelModal = true;
            ImGui::OpenPopup("Unsaved Level Changes");
            return;
        }

        switch (g_PendingLevelAction.type)
        {
        case PendingLevelActionType::NewLevel:
            CreateNewEditorLevel();
            break;
        case PendingLevelActionType::OpenLevelPath:
            LoadEditorLevelFromPath(g_PendingLevelAction.path);
            break;
        case PendingLevelActionType::OpenLevelDialog:
        {
            std::wstring selectedPath;
            if (OpenLevelDialog(AppBase::Get().GetMainWindow()->GetHwnd(), selectedPath, GetPreferredLevelDirectoryW()))
            {
                LoadEditorLevelFromPath(WideToUtf8(selectedPath));
            }
            break;
        }
        default:
            break;
        }

        g_BypassUnsavedLevelCheckOnce = false;
        g_PendingLevelAction = {};
    }

    static void RenderUnsavedLevelModal()
    {
        if (!g_ShowUnsavedLevelModal)
        {
            return;
        }

        if (ImGui::BeginPopupModal("Unsaved Level Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Current level has unsaved changes.");
            ImGui::TextDisabled("Save before continuing to New/Open?");
            ImGui::Separator();

            if (ImGui::Button("Save and Continue", ImVec2(180.0f, 0.0f)))
            {
                bool bSaved = false;
                if (GEditor.document.currentLevelPath.empty())
                {
                    std::wstring selectedPath;
                    if (SaveLevelDialog(AppBase::Get().GetMainWindow()->GetHwnd(), selectedPath, GetPreferredLevelDirectoryW(), GetSuggestedLevelFilenameW()))
                    {
                        bSaved = SaveEditorLevelToPath(WideToUtf8(selectedPath));
                    }
                }
                else
                {
                    bSaved = SaveEditorLevelToPath(GEditor.document.currentLevelPath);
                }

                if (bSaved)
                {
                    g_ShowUnsavedLevelModal = false;
                    ImGui::CloseCurrentPopup();
                    PendingLevelAction queued = g_PendingLevelAction;
                    g_PendingLevelAction = {};
                    QueuePendingLevelAction(queued.type, queued.path);
                }
            }
            if (ImGui::Button("Continue Without Saving", ImVec2(180.0f, 0.0f)))
            {
                g_ShowUnsavedLevelModal = false;
                ImGui::CloseCurrentPopup();
                PendingLevelAction queued = g_PendingLevelAction;
                g_PendingLevelAction = {};
                g_BypassUnsavedLevelCheckOnce = true;
                QueuePendingLevelAction(queued.type, queued.path);
            }
            if (ImGui::Button("Cancel", ImVec2(180.0f, 0.0f)))
            {
                g_ShowUnsavedLevelModal = false;
                g_PendingLevelAction = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    struct RootMotionRuntimeValidationOutput
    {
        bool bExecuted = false;
        bool bPass = false;
        std::string Summary;
    };

    static float QuatDiffDeg_Runtime(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b)
    {
        using namespace DirectX;
        XMFLOAT4 qa{}, qb{};
        XMStoreFloat4(&qa, XMQuaternionNormalize(a));
        XMStoreFloat4(&qb, XMQuaternionNormalize(b));
        float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
        dot = (std::max)(-1.0f, (std::min)(1.0f, std::fabs(dot)));
        return XMConvertToDegrees(2.0f * std::acos(dot));
    }

    static void ComputeBoneWorldTransforms_Runtime(
        const Character& actor,
        const SkeletalMeshComponent& sk,
        const AnimInstance& anim,
        const Skeleton& skeleton,
        std::vector<DirectX::XMFLOAT3>& outPos,
        std::vector<DirectX::XMFLOAT4>& outRot,
        std::vector<uint8_t>& outValid)
    {
        using namespace DirectX;

        const size_t boneCount = skeleton.Bones.size();
        outPos.assign(boneCount, XMFLOAT3{});
        outRot.assign(boneCount, XMFLOAT4{0.f, 0.f, 0.f, 1.f});
        outValid.assign(boneCount, 0);

        if (boneCount == 0 || skeleton.Nodes.empty())
        {
            return;
        }

        const std::vector<XMFLOAT4X4>& localPose = anim.GetLocalPose();
        std::vector<XMFLOAT4X4> nodeLocal(skeleton.Nodes.size());
        std::vector<XMFLOAT4X4> nodeGlobal(skeleton.Nodes.size());
        std::vector<uint8_t> visited(skeleton.Nodes.size(), 0);

        for (size_t ni = 0; ni < skeleton.Nodes.size(); ++ni)
        {
            nodeLocal[ni] = skeleton.Nodes[ni].RefLocal;
            XMStoreFloat4x4(&nodeGlobal[ni], XMMatrixIdentity());
        }

        for (size_t bi = 0; bi < skeleton.Bones.size(); ++bi)
        {
            if (bi >= skeleton.BoneToNode.size() || bi >= localPose.size())
            {
                continue;
            }
            const int32_t nodeIdx = skeleton.BoneToNode[bi];
            if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= skeleton.Nodes.size())
            {
                continue;
            }
            nodeLocal[static_cast<size_t>(nodeIdx)] = localPose[bi];
        }

        std::function<void(int32_t)> BuildNodeGlobal = [&](int32_t idx)
        {
            if (idx < 0 || static_cast<size_t>(idx) >= skeleton.Nodes.size())
            {
                return;
            }
            if (visited[static_cast<size_t>(idx)])
            {
                return;
            }

            const int32_t parent = skeleton.Nodes[static_cast<size_t>(idx)].Parent;
            if (parent >= 0)
            {
                BuildNodeGlobal(parent);
                const XMMATRIX MLocal = XMLoadFloat4x4(&nodeLocal[static_cast<size_t>(idx)]);
                const XMMATRIX MParent = XMLoadFloat4x4(&nodeGlobal[static_cast<size_t>(parent)]);
                XMStoreFloat4x4(&nodeGlobal[static_cast<size_t>(idx)], MLocal * MParent);
            }
            else
            {
                nodeGlobal[static_cast<size_t>(idx)] = nodeLocal[static_cast<size_t>(idx)];
            }

            visited[static_cast<size_t>(idx)] = 1;
        };

        for (int32_t ni = 0; ni < static_cast<int32_t>(skeleton.Nodes.size()); ++ni)
        {
            BuildNodeGlobal(ni);
        }

        const XMMATRIX MMeshRel = sk.GetRelativeTransform().ToMatrix();
        const XMMATRIX MGlobalInv = XMLoadFloat4x4(&skeleton.GlobalInverse);
        const SceneComponent* root = actor.GetRootComponent();
        const XMMATRIX MActorRoot = root ? root->GetRelativeTransform().ToMatrix() : XMMatrixIdentity();

        for (size_t bi = 0; bi < boneCount; ++bi)
        {
            if (bi >= skeleton.BoneToNode.size())
            {
                continue;
            }

            const int32_t boneNode = skeleton.BoneToNode[bi];
            if (boneNode < 0 || static_cast<size_t>(boneNode) >= skeleton.Nodes.size())
            {
                continue;
            }

            const XMMATRIX MBone = XMLoadFloat4x4(&nodeGlobal[static_cast<size_t>(boneNode)]);
            // Render path alignment:
            // vertexWS = vertex * (Offset * Global * GlobalInverse) * MeshRel * ActorRoot
            // bone-origin 비교는 Offset을 제외한 Global*GlobalInverse*MeshRel*ActorRoot로 맞춘다.
            const XMMATRIX MBoneWorld = MBone * MGlobalInv * MMeshRel * MActorRoot;

            XMVECTOR S, R, T;
            if (!XMMatrixDecompose(&S, &R, &T, MBoneWorld))
            {
                continue;
            }

            XMStoreFloat3(&outPos[bi], T);
            XMStoreFloat4(&outRot[bi], XMQuaternionNormalize(R));
            outValid[bi] = 1;
        }
    }

    static RootMotionRuntimeValidationOutput RunRootMotionRuntimeEquivalenceValidation(
        Character* sourceCharacter,
        int totalFrames,
        bool bDumpCsv)
    {
        using namespace DirectX;

        RootMotionRuntimeValidationOutput out{};
        if (!sourceCharacter)
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor is null.";
            return out;
        }

        SkeletalMeshComponent* sourceSk = sourceCharacter->GetSkeletalComponent();
        if (!sourceSk)
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor has no SkeletalMeshComponent.";
            return out;
        }

        AnimInstance* sourceAnim = sourceSk->GetAnimInstance();
        if (!sourceAnim)
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor has no AnimInstance.";
            return out;
        }

        const std::shared_ptr<SkeletalMeshAsset>& meshShared = sourceSk->GetMeshShared();
        if (!meshShared)
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor has no skeletal mesh asset.";
            return out;
        }

        const std::shared_ptr<AnimSequenceAsset>& seqSrc = sourceAnim->GetSequence();
        if (!seqSrc || seqSrc->Sections.empty())
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor has no animation sequence.";
            return out;
        }

        const Skeleton* sourceSkeleton = sourceSk->GetSkeleton();
        if (!sourceSkeleton)
        {
            out.Summary = "[RuntimeValidate][Error] Selected actor has no skeleton.";
            return out;
        }

        const Transform rootSource = sourceCharacter->GetRootComponent()->GetRelativeTransform();
        const Transform meshSource = sourceSk->GetRelativeTransform();
        const std::string sectionName = sourceAnim->GetCurrentSectionName();
        const bool bLoop = sourceAnim->IsLooping();
        const float playRate = sourceAnim->GetPlayRate();
        const float sourceStartTime = sourceAnim->GetTimeInSection();

        Character actorOn;
        Character actorOff;
        actorOn.SetName("RuntimeRM_On");
        actorOff.SetName("RuntimeRM_Off");

        if (!actorOn.GetRootComponent() || !actorOff.GetRootComponent())
        {
            out.Summary = "[RuntimeValidate][Error] Internal actor root is null.";
            return out;
        }

        actorOn.GetRootComponent()->GetRelativeTransform() = rootSource;
        actorOff.GetRootComponent()->GetRelativeTransform() = rootSource;

        SkeletalMeshComponent* skOn = actorOn.GetSkeletalComponent();
        SkeletalMeshComponent* skOff = actorOff.GetSkeletalComponent();
        if (!skOn || !skOff)
        {
            out.Summary = "[RuntimeValidate][Error] Internal SkeletalMeshComponent is null.";
            return out;
        }

        skOn->SetMesh(meshShared);
        skOff->SetMesh(meshShared);
        skOn->GetRelativeTransform() = meshSource;
        skOff->GetRelativeTransform() = meshSource;

        AnimInstance* animOn = skOn->GetAnimInstance();
        AnimInstance* animOff = skOff->GetAnimInstance();
        if (!animOn || !animOff)
        {
            out.Summary = "[RuntimeValidate][Error] Internal AnimInstance is null.";
            return out;
        }

        std::shared_ptr<AnimSequenceAsset> seqOn = std::make_shared<AnimSequenceAsset>(*seqSrc);
        std::shared_ptr<AnimSequenceAsset> seqOff = std::make_shared<AnimSequenceAsset>(*seqSrc);
        for (AnimSection& sec : seqOn->Sections) { sec.bEnableRootMotion = true; }
        for (AnimSection& sec : seqOff->Sections) { sec.bEnableRootMotion = false; }

        animOn->SetConsumeRootInPose(true);
        animOn->SetSequence(seqOn);
        animOn->Play(sectionName, bLoop, playRate);

        animOff->SetConsumeRootInPose(false);
        animOff->SetSequence(seqOff);
        animOff->Play(sectionName, bLoop, playRate);

        CharacterMovementComponent* moveOn = actorOn.GetMovementComponent();
        CharacterMovementComponent* moveOff = actorOff.GetMovementComponent();
        if (!moveOn || !moveOff)
        {
            out.Summary = "[RuntimeValidate][Error] Internal CharacterMovementComponent is null.";
            return out;
        }

        moveOn->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
        moveOff->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

        constexpr float dt = 1.0f / 60.0f;
        constexpr float kPosTol = 0.01f;
        constexpr float kRotTolDeg = 0.2f;
        constexpr float kRelRotTolDeg = 0.2f;

        // 선택 액터의 현재 섹션 시간부터 비교하려면 두 복제 액터를 같은 시간으로 맞춘다.
        if (sourceStartTime > 1e-6f)
        {
            const int warmupFrames = (std::max)(1, static_cast<int>(std::floor(sourceStartTime / dt)));
            for (int i = 0; i < warmupFrames; ++i)
            {
                actorOn.Tick(dt);
                actorOff.Tick(dt);
            }
        }

        const Skeleton* skeleton = skOn->GetSkeleton();
        if (!skeleton)
        {
            out.Summary = "[RuntimeValidate][Error] Internal skeleton is null.";
            return out;
        }

        std::ofstream csvFile;
        if (bDumpCsv)
        {
            csvFile.open(kRuntimeValidationCsvPath, std::ios::out | std::ios::trunc);
            if (csvFile.is_open())
            {
                csvFile << "frame,bone_index,bone_name,pos_diff,rot_diff_deg,rel_rot_diff_deg\n";
            }
        }

        std::vector<XMFLOAT3> onPos, offPos;
        std::vector<XMFLOAT4> onRot, offRot;
        std::vector<uint8_t> onValid, offValid;
        std::vector<XMFLOAT4> qOnBase(skeleton->Bones.size(), XMFLOAT4{0.f, 0.f, 0.f, 1.f});
        std::vector<XMFLOAT4> qOffBase(skeleton->Bones.size(), XMFLOAT4{0.f, 0.f, 0.f, 1.f});
        std::vector<uint8_t> hasBase(skeleton->Bones.size(), 0);

        float maxPosDiff = 0.0f;
        float maxRotDiff = 0.0f;
        float maxRelRotDiff = 0.0f;
        int maxPosFrame = 0;
        int maxRotFrame = 0;
        int maxRelRotFrame = 0;
        size_t maxPosBone = 0;
        size_t maxRotBone = 0;
        size_t maxRelRotBone = 0;

        for (int frame = 1; frame <= totalFrames; ++frame)
        {
            actorOn.Tick(dt);
            actorOff.Tick(dt);

            ComputeBoneWorldTransforms_Runtime(actorOn, *skOn, *animOn, *skeleton, onPos, onRot, onValid);
            ComputeBoneWorldTransforms_Runtime(actorOff, *skOff, *animOff, *skeleton, offPos, offRot, offValid);
            const size_t n = (std::min)(onRot.size(), offRot.size());
            for (size_t bi = 0; bi < n; ++bi)
            {
                if (bi >= onValid.size() || bi >= offValid.size())
                {
                    continue;
                }
                if (onValid[bi] == 0 || offValid[bi] == 0)
                {
                    continue;
                }

                const XMVECTOR qOn = XMQuaternionNormalize(XMLoadFloat4(&onRot[bi]));
                const XMVECTOR qOff = XMQuaternionNormalize(XMLoadFloat4(&offRot[bi]));
                const float rotDiffDeg = QuatDiffDeg_Runtime(qOn, qOff);
                if (rotDiffDeg > maxRotDiff)
                {
                    maxRotDiff = rotDiffDeg;
                    maxRotBone = bi;
                    maxRotFrame = frame;
                }

                if (!hasBase[bi])
                {
                    XMStoreFloat4(&qOnBase[bi], qOn);
                    XMStoreFloat4(&qOffBase[bi], qOff);
                    hasBase[bi] = 1;
                }
                const XMVECTOR qOnRel = XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&qOnBase[bi])), qOn);
                const XMVECTOR qOffRel = XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&qOffBase[bi])), qOff);
                const float relRotDiffDeg = QuatDiffDeg_Runtime(qOnRel, qOffRel);
                if (relRotDiffDeg > maxRelRotDiff)
                {
                    maxRelRotDiff = relRotDiffDeg;
                    maxRelRotBone = bi;
                    maxRelRotFrame = frame;
                }

                const float dx = onPos[bi].x - offPos[bi].x;
                const float dy = onPos[bi].y - offPos[bi].y;
                const float dz = onPos[bi].z - offPos[bi].z;
                const float posDiff = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (posDiff > maxPosDiff)
                {
                    maxPosDiff = posDiff;
                    maxPosBone = bi;
                    maxPosFrame = frame;
                }

                if (csvFile.is_open())
                {
                    const std::string boneName = (bi < skeleton->Bones.size()) ? skeleton->Bones[bi].Name : std::string("N/A");
                    csvFile << frame << "," << bi << ",\"" << boneName << "\","
                            << std::fixed << std::setprecision(6)
                            << posDiff << "," << rotDiffDeg << "," << relRotDiffDeg << "\n";
                }
            }
        }

        const bool bPass = (maxPosDiff <= kPosTol) && (maxRotDiff <= kRotTolDeg) && (maxRelRotDiff <= kRelRotTolDeg);
        out.bExecuted = true;
        out.bPass = bPass;

        const std::string posBoneName = (maxPosBone < skeleton->Bones.size()) ? skeleton->Bones[maxPosBone].Name : std::string("N/A");
        const std::string rotBoneName = (maxRotBone < skeleton->Bones.size()) ? skeleton->Bones[maxRotBone].Name : std::string("N/A");
        const std::string relRotBoneName = (maxRelRotBone < skeleton->Bones.size()) ? skeleton->Bones[maxRelRotBone].Name : std::string("N/A");

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << "[RuntimeValidate][FullDelta] " << (bPass ? "PASS" : "FAIL")
            << " PosMax=" << maxPosDiff << " (Bone=" << maxPosBone << ":" << posBoneName << ", Frame=" << maxPosFrame << ")"
            << " RotMaxDeg=" << maxRotDiff << " (Bone=" << maxRotBone << ":" << rotBoneName << ", Frame=" << maxRotFrame << ")"
            << " RelRotMaxDeg=" << maxRelRotDiff << " (Bone=" << maxRelRotBone << ":" << relRotBoneName << ", Frame=" << maxRelRotFrame << ")"
            << " Thresholds(Pos=" << kPosTol << ",RotDeg=" << kRotTolDeg << ",RelRotDeg=" << kRelRotTolDeg << ")";
        out.Summary = oss.str();
        AppendRuntimeValidationLogLine(out.Summary);
        if (csvFile.is_open())
        {
            AppendRuntimeValidationLogLine(std::string("[RuntimeValidate] CSV: ") + kRuntimeValidationCsvPath);
        }

        return out;
    }

    static std::string g_RuntimeValidationStatus = "Not executed.";
    static bool g_RuntimeValidationDumpCsv = false;
    static int g_RuntimeValidationFrames = 1200;
    static std::string g_RootMotionPairSetupStatus = "Not executed.";
    static float g_RootMotionPairSpacingX = 4.0f;

    static void RunRuntimeValidationForCurrentSelectionIfCharacter()
    {
        std::shared_ptr<Actor> selected = GEditor.selection.SelectedActor.lock();
        if (!selected)
        {
            g_RuntimeValidationStatus = "[RuntimeValidate] No selection.";
            AppendRuntimeValidationLogLine(g_RuntimeValidationStatus);
            return;
        }

        Character* selectedCharacter = dynamic_cast<Character*>(selected.get());
        if (!selectedCharacter)
        {
            g_RuntimeValidationStatus = "[RuntimeValidate] Selected actor is not Character.";
            AppendRuntimeValidationLogLine(g_RuntimeValidationStatus + std::string(" Name=") + selected->GetName());
            return;
        }

        const int frames = (std::max)(60, g_RuntimeValidationFrames);
        const RootMotionRuntimeValidationOutput result =
            RunRootMotionRuntimeEquivalenceValidation(selectedCharacter, frames, g_RuntimeValidationDumpCsv);
        g_RuntimeValidationStatus = result.Summary;
    }

    static std::shared_ptr<Actor> FindSharedActorByRaw(World* world, const Actor* raw)
    {
        if (!world || !raw)
        {
            return nullptr;
        }

        for (const std::shared_ptr<Actor>& actor : world->GetActors())
        {
            if (actor && actor.get() == raw)
            {
                return actor;
            }
        }
        return nullptr;
    }

    static void SetupRootMotionPairForCurrentWorld()
    {
        if (!GEditor.world)
        {
            g_RootMotionPairSetupStatus = "[RootMotionTestSetup][Error] World is null.";
            AppendRuntimeValidationLogLine(g_RootMotionPairSetupStatus);
            return;
        }

        Test::RootMotionPairSetupOptions options{};
        options.PairSpacingX = (std::max)(0.5f, g_RootMotionPairSpacingX);
        options.bUseCustomActorBaseTransform = true;
        options.ActorBasePos = {0.0f, 0.0f, 0.0f};
        options.ActorBaseRotEulerRad = {1.6f, 0.0f, 0.0f};
        options.ActorBaseScale = {0.02f, 0.02f, 0.02f};

        const Test::RootMotionPairSetupResult result =
            Test::SetupRootMotionOnOffPairInWorld(*GEditor.world, options);
        g_RootMotionPairSetupStatus = result.Message;
        AppendRuntimeValidationLogLine(g_RootMotionPairSetupStatus);

        if (result.bSuccess && result.OnActor)
        {
            if (std::shared_ptr<Actor> onShared = FindSharedActorByRaw(GEditor.world, result.OnActor))
            {
                GEditor.selection.SelectedActor = onShared;
            }
        }
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

    LoadRecentLevelsFromDisk();

    return true;
}

void ImGuiHandler::ShoutDown()
{
    SaveRecentLevelsToDisk();
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

    constexpr float kToolbarHeight = 34.0f;
    if (ImGui::BeginChild(
        "##ViewportToolbar",
        ImVec2(0.0f, kToolbarHeight),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        RenderSettings& rs = RenderSettings::Get();

        int drawMode = 0; // 0=Lit, 1=Wireframe, 2=NormalVector
        if (rs.drawNormalVector)
        {
            drawMode = 2;
        }
        else if (rs.drawAsWire)
        {
            drawMode = 1;
        }

        if (ImGui::RadioButton("Loc (W)", g_ViewportGizmoMode == ViewportGizmoMode::Translate))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Translate;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rot (E)", g_ViewportGizmoMode == ViewportGizmoMode::Rotate))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Rotate;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale (R)", g_ViewportGizmoMode == ViewportGizmoMode::Scale))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Scale;
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("| Draw:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Lit", drawMode == 0))
        {
            drawMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Wireframe", drawMode == 1))
        {
            drawMode = 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("NormalVector", drawMode == 2))
        {
            drawMode = 2;
        }

        rs.drawAsWire = (drawMode == 1);
        rs.drawNormalVector = (drawMode == 2);

        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        ImGui::Checkbox("FootIK Debug", &g_ShowFootIKDebugOverlay);
        if (g_ShowFootIKDebugOverlay)
        {
            FootIKSampleTuning& footIkTuning = FootIKSample::GetTuning();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::DragFloat("Up##FootIK", &footIkTuning.TraceUpDistance, 0.2f, 1.0f, 120.0f, "Up %.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragFloat("Down##FootIK", &footIkTuning.TraceDownDistance, 0.2f, 1.0f, 200.0f, "Down %.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(88.0f);
            ImGui::DragFloat("HeelToe##FootIK", &footIkTuning.HeelToeSampleDistance, 0.1f, 0.0f, 30.0f, "HT %.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragFloat("Pelvis##FootIK", &footIkTuning.MaxPelvisOffsetDown, 0.2f, 1.0f, 120.0f, "Pelvis %.1f");
            ImGui::SameLine();
            ImGui::Checkbox("Apply##FootIK", &footIkTuning.bApplyRuntimeCorrection);
            ImGui::SameLine();
            ImGui::Checkbox("Normal##FootIK", &footIkTuning.bEnableFootOrientation);
            ImGui::SameLine();
            ImGui::Checkbox("Lock##FootIK", &footIkTuning.bEnableFootLock);
            ImGui::SameLine();
            ImGui::Checkbox("2Bone##FootIK", &footIkTuning.bEnableTwoBoneSolver);
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        if (ImGui::Button("New Level"))
        {
            QueuePendingLevelAction(PendingLevelActionType::NewLevel);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Level"))
        {
            QueuePendingLevelAction(PendingLevelActionType::OpenLevelDialog);
        }
        ImGui::SameLine();
        if (ImGui::BeginCombo("##recent_levels", "Recent Levels"))
        {
            if (GEditor.document.recentLevelPaths.empty())
            {
                ImGui::TextDisabled("No recent levels yet.");
            }
            for (const std::string& recentPath : GEditor.document.recentLevelPaths)
            {
                const std::string displayPath = ToDisplayLevelPath(recentPath);
                const std::string label = std::filesystem::path(recentPath).filename().string() + "##" + recentPath;
                if (ImGui::Selectable(label.c_str()))
                {
                    QueuePendingLevelAction(PendingLevelActionType::OpenLevelPath, recentPath);
                }
                ImGui::TextDisabled("%s", displayPath.c_str());
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Level"))
        {
            if (GEditor.document.currentLevelPath.empty())
            {
                std::wstring selectedPath;
                if (SaveLevelDialog(AppBase::Get().GetMainWindow()->GetHwnd(), selectedPath, GetPreferredLevelDirectoryW(), GetSuggestedLevelFilenameW()))
                {
                    SaveEditorLevelToPath(WideToUtf8(selectedPath));
                }
            }
            else
            {
                SaveEditorLevelToPath(GEditor.document.currentLevelPath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As"))
        {
            std::wstring selectedPath;
            if (SaveLevelDialog(AppBase::Get().GetMainWindow()->GetHwnd(), selectedPath, GetPreferredLevelDirectoryW(), GetSuggestedLevelFilenameW()))
            {
                SaveEditorLevelToPath(WideToUtf8(selectedPath));
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        const char* dirtyLabel = GEditor.document.bLevelDirty ? "Dirty" : "Saved";
        const ImVec4 dirtyColor = GEditor.document.bLevelDirty
            ? ImVec4(0.92f, 0.63f, 0.20f, 1.0f)
            : ImVec4(0.40f, 0.84f, 0.48f, 1.0f);
        ImGui::TextColored(dirtyColor, "%s", dirtyLabel);
        ImGui::SameLine();
        const std::string levelLabel = GEditor.document.currentLevelPath.empty()
            ? (GEditor.document.currentLevelName.empty() ? std::string("Unsaved Level") : GEditor.document.currentLevelName + " (unsaved path)")
            : ToDisplayLevelPath(GEditor.document.currentLevelPath);
        ImGui::TextDisabled("%s", levelLabel.c_str());
    }
    ImGui::EndChild();

    RenderUnsavedLevelModal();

    if (!GEditor.document.statusMessage.empty())
    {
        ImGui::TextDisabled("%s", GEditor.document.statusMessage.c_str());
    }
    ImGui::Spacing();

    RenderViewportContents();
    ImGui::End();
}

void ImGuiHandler::RenderViewportContents()
{
    GEditor.viewportInput.bHovered = false;
    GEditor.viewportInput.bFocused = false;
    GEditor.viewportInput.bMouseCaptured = false;

    static int s_ActiveAxis = -1; // 0:X, 1:Y, 2:Z
    static bool s_IsDraggingAxis = false;
    static ImVec2 s_LastMousePos(0.0f, 0.0f);
    static uintptr_t s_DraggingActorKey = 0;

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
    const ImVec2 imgMin = ImGui::GetItemRectMin();
    const ImVec2 imgMax = ImGui::GetItemRectMax();
    const bool viewportHovered = ImGui::IsItemHovered();
    const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    GEditor.viewportInput.bHovered = viewportHovered;
    GEditor.viewportInput.bFocused = viewportFocused;
    GEditor.viewportInput.bMouseCaptured = viewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

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

    if (!GEditor.world)
    {
        return;
    }

    Scene& scene = GEditor.world->GetScene();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (g_ShowFootIKDebugOverlay)
    {
        for (const std::shared_ptr<Actor>& actor : GEditor.world->GetActors())
        {
            if (!actor)
            {
                continue;
            }

            const Character* character = dynamic_cast<const Character*>(actor.get());
            if (!character)
            {
                continue;
            }

            const SkeletalMeshComponent* sk = character->GetSkeletalComponent();
            if (!sk)
            {
                continue;
            }

            const FootIKSampleState& footState = sk->GetFootIKSampleState();
            if (!footState.bHasValidBones)
            {
                continue;
            }

            auto DrawTrace = [&](const FootIKTraceDebugSample& sample, ImU32 rayColor, ImU32 hitColor)
            {
                ImVec2 start2D{}, end2D{};
                if (ProjectWorldPointToViewport(scene, sample.TraceStart, imgMin, imgMax, uv0, uv1, start2D) &&
                    ProjectWorldPointToViewport(scene, sample.TraceEnd, imgMin, imgMax, uv0, uv1, end2D))
                {
                    drawList->AddLine(start2D, end2D, rayColor, 1.6f);
                }

                if (!sample.bHit) return;

                ImVec2 hit2D{};
                if (ProjectWorldPointToViewport(scene, sample.HitLocation, imgMin, imgMax, uv0, uv1, hit2D))
                {
                    drawList->AddCircleFilled(hit2D, 3.0f, hitColor, 10);
                }

                DirectX::XMFLOAT3 normalEnd = sample.HitLocation;
                normalEnd.x += sample.HitNormal.x * 14.0f;
                normalEnd.y += sample.HitNormal.y * 14.0f;
                normalEnd.z += sample.HitNormal.z * 14.0f;

                ImVec2 normalEnd2D{};
                if (ProjectWorldPointToViewport(scene, sample.HitLocation, imgMin, imgMax, uv0, uv1, hit2D) &&
                    ProjectWorldPointToViewport(scene, normalEnd, imgMin, imgMax, uv0, uv1, normalEnd2D))
                {
                    drawList->AddLine(hit2D, normalEnd2D, IM_COL32(120, 220, 255, 255), 1.6f);
                }
            };

            auto DrawFootSamples = [&](const FootIKContactDebug& contact, ImU32 baseRay, ImU32 baseHit)
            {
                DrawTrace(contact.Center, baseRay, baseHit);
                DrawTrace(contact.Heel, IM_COL32((baseRay >> IM_COL32_R_SHIFT) & 0xFF, (baseRay >> IM_COL32_G_SHIFT) & 0xFF, (baseRay >> IM_COL32_B_SHIFT) & 0xFF, 140), baseHit);
                DrawTrace(contact.Toe, IM_COL32((baseRay >> IM_COL32_R_SHIFT) & 0xFF, (baseRay >> IM_COL32_G_SHIFT) & 0xFF, (baseRay >> IM_COL32_B_SHIFT) & 0xFF, 140), baseHit);
            };

            DrawFootSamples(footState.Left, IM_COL32(100, 255, 120, 220), IM_COL32(20, 220, 80, 255));
            DrawFootSamples(footState.Right, IM_COL32(255, 160, 100, 220), IM_COL32(255, 120, 60, 255));

            auto DrawFootAppliedText = [&](const FootIKContactDebug& contact, float rawTarget, float appliedEffector, bool bOrientationApplied, bool bLocked,
                                         bool bSolver, bool bFallback, bool bKneeHint, bool bToeHeel, float reachRatio,
                                         const std::string& quality, const std::string& fallbackReason, ImU32 color)
            {
                if (!contact.bAnyHit) return;

                ImVec2 hit2D{};
                if (!ProjectWorldPointToViewport(scene, contact.Center.HitLocation, imgMin, imgMax, uv0, uv1, hit2D)) return;

                char footBuf[320]{};
                std::snprintf(footBuf, sizeof(footBuf), "raw %.2f / app %.2f | %s%s%s | Q:%s RR:%.2f %s%s%s",
                    rawTarget,
                    appliedEffector,
                    bOrientationApplied ? "N " : "",
                    bLocked ? "Lock " : "",
                    bSolver ? "2B" : (bFallback ? "FB" : ""),
                    quality.empty() ? "na" : quality.c_str(),
                    reachRatio,
                    bKneeHint ? "KH " : "",
                    bToeHeel ? "TH " : "",
                    (!bSolver && !fallbackReason.empty()) ? fallbackReason.c_str() : "");
                drawList->AddText(ImVec2(hit2D.x + 8.0f, hit2D.y - 8.0f), color, footBuf);
            };

            DrawFootAppliedText(footState.Left, footState.LeftRawTargetEffectorOffsetY, footState.LeftAppliedEffectorOffsetY,
                footState.bLeftOrientationApplied, footState.LeftLock.bLocked, footState.bLeftSolverUsed, footState.bLeftSolverFallbackUsed,
                footState.bLeftKneeHintUsed, footState.bLeftToeHeelProfileUsed, footState.LeftSolverReachRatio,
                footState.LeftSolverQualityState, footState.LeftSolverFallbackReason, IM_COL32(170, 255, 170, 255));
            DrawFootAppliedText(footState.Right, footState.RightRawTargetEffectorOffsetY, footState.RightAppliedEffectorOffsetY,
                footState.bRightOrientationApplied, footState.RightLock.bLocked, footState.bRightSolverUsed, footState.bRightSolverFallbackUsed,
                footState.bRightKneeHintUsed, footState.bRightToeHeelProfileUsed, footState.RightSolverReachRatio,
                footState.RightSolverQualityState, footState.RightSolverFallbackReason, IM_COL32(255, 190, 170, 255));

            ImVec2 pelvisLabelPos{};
            DirectX::XMFLOAT3 labelWorld = actor->GetRootComponent() ? actor->GetRootComponent()->GetWorldLocation() : DirectX::XMFLOAT3{};
            labelWorld.y += 24.0f;
            if (ProjectWorldPointToViewport(scene, labelWorld, imgMin, imgMax, uv0, uv1, pelvisLabelPos))
            {
                char buf[256]{};
                const char* chainSource = footState.bUsingProfileBoneChain ? "PF" : (footState.bUsingHeuristicBoneChainFallback ? "HF" : "NA");
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "Pelvis tgt/sm/ap %.2f/%.2f/%.2f | Chain %s(%s) | L %.2f %s %s | R %.2f %s %s",
                    footState.TargetPelvisOffsetY,
                    footState.SmoothedPelvisOffsetY,
                    footState.AppliedPelvisOffsetY,
                    chainSource,
                    footState.ActiveBoneChainProfileKey.empty() ? "-" : footState.ActiveBoneChainProfileKey.c_str(),
                    footState.LeftAppliedEffectorOffsetY,
                    footState.LeftLock.bLocked ? "Lock" : "",
                    footState.LeftSolverFallbackReason.empty() ? "" : footState.LeftSolverFallbackReason.c_str(),
                    footState.RightAppliedEffectorOffsetY,
                    footState.RightLock.bLocked ? "Lock" : "",
                    footState.RightSolverFallbackReason.empty() ? "" : footState.RightSolverFallbackReason.c_str());
                drawList->AddText(pelvisLabelPos, IM_COL32(240, 240, 140, 255), buf);
            }
        }
    }

    if (viewportHovered)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Translate;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Rotate;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R))
        {
            g_ViewportGizmoMode = ViewportGizmoMode::Scale;
        }
    }

    std::shared_ptr<Actor> selectedActor = GEditor.selection.SelectedActor.lock();
    SceneComponent* selectedRoot = selectedActor ? selectedActor->GetRootComponent() : nullptr;
    if (!selectedActor || !selectedRoot)
    {
        s_IsDraggingAxis = false;
        s_ActiveAxis = -1;
        s_DraggingActorKey = 0;
    }

    bool hasValidGizmo = false;
    int hoveredAxis = -1;
    ImVec2 gizmoPivot2D(0.0f, 0.0f);
    ImVec2 gizmoAxisEnd2D[3] = {};
    DirectX::XMVECTOR gizmoAxisWorld[3] = {};
    float cameraDistance = 1.0f;

    if (selectedRoot)
    {
        using namespace DirectX;
        const XMMATRIX world = selectedRoot->GetWorldMatrix();
        const XMVECTOR pivot = world.r[3];
        XMFLOAT3 pivot3;
        XMStoreFloat3(&pivot3, pivot);

        if (ProjectWorldPointToViewport(scene, pivot3, imgMin, imgMax, uv0, uv1, gizmoPivot2D))
        {
            const XMFLOAT3 camPos = scene.GetViewInfo().cameraPosition;
            const XMVECTOR camV = XMLoadFloat3(&camPos);
            cameraDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(pivot, camV)));
            if (cameraDistance < 0.5f)
            {
                cameraDistance = 0.5f;
            }

            auto safeNormalize = [](DirectX::FXMVECTOR v, DirectX::FXMVECTOR fallback) -> DirectX::XMVECTOR
            {
                using namespace DirectX;
                const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
                if (lenSq <= 1e-8f)
                {
                    return fallback;
                }
                return XMVector3Normalize(v);
            };

            gizmoAxisWorld[0] = safeNormalize(world.r[0], XMVectorSet(1, 0, 0, 0));
            gizmoAxisWorld[1] = safeNormalize(world.r[1], XMVectorSet(0, 1, 0, 0));
            gizmoAxisWorld[2] = safeNormalize(world.r[2], XMVectorSet(0, 0, 1, 0));

            const float handleLength = cameraDistance * 0.18f;
            int projectedAxisCount = 0; // Translate/Scale에서만 사용
            if (g_ViewportGizmoMode != ViewportGizmoMode::Rotate)
            {
                for (int i = 0; i < 3; ++i)
                {
                    XMFLOAT3 end3;
                    XMStoreFloat3(&end3, XMVectorAdd(pivot, XMVectorScale(gizmoAxisWorld[i], handleLength)));
                    if (ProjectWorldPointToViewport(scene, end3, imgMin, imgMax, uv0, uv1, gizmoAxisEnd2D[i]))
                    {
                        projectedAxisCount++;
                    }
                    else
                    {
                        gizmoAxisEnd2D[i] = gizmoPivot2D;
                    }
                }
            }
            hasValidGizmo = (g_ViewportGizmoMode == ViewportGizmoMode::Rotate) ? true : (projectedAxisCount >= 2);

            if (hasValidGizmo)
            {
                const ImU32 axisColors[3] =
                {
                    IM_COL32(242, 72, 72, 255),
                    IM_COL32(80, 220, 90, 255),
                    IM_COL32(88, 150, 255, 255),
                };
                const float rotateRadiusWorld = cameraDistance * 0.20f;
                const int ringSegments = 72;
                const float scaleBoxHalf = 6.0f;

                auto BuildRotateRingBasis = [&](int axisIndex, XMVECTOR& outU, XMVECTOR& outV)
                {
                    XMVECTOR n = gizmoAxisWorld[axisIndex];
                    XMVECTOR seed = gizmoAxisWorld[(axisIndex + 1) % 3];
                    seed = XMVectorSubtract(seed, XMVectorScale(n, XMVectorGetX(XMVector3Dot(seed, n))));
                    if (XMVectorGetX(XMVector3LengthSq(seed)) <= 1e-6f)
                    {
                        seed = (axisIndex == 1) ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
                    }
                    outU = XMVector3Normalize(seed);
                    outV = XMVector3Normalize(XMVector3Cross(n, outU));
                };

                if (viewportHovered)
                {
                    if (g_ViewportGizmoMode == ViewportGizmoMode::Rotate)
                    {
                        float bestD2 = FLT_MAX;
                        for (int i = 0; i < 3; ++i)
                        {
                            XMVECTOR ringU;
                            XMVECTOR ringV;
                            BuildRotateRingBasis(i, ringU, ringV);

                            ImVec2 prev2D(0.0f, 0.0f);
                            bool prevValid = false;
                            for (int s = 0; s <= ringSegments; ++s)
                            {
                                const float a = (6.28318530718f * (float)s) / (float)ringSegments;
                                const XMVECTOR p = XMVectorAdd(pivot, XMVectorAdd(
                                    XMVectorScale(ringU, std::cos(a) * rotateRadiusWorld),
                                    XMVectorScale(ringV, std::sin(a) * rotateRadiusWorld)));

                                XMFLOAT3 p3;
                                XMStoreFloat3(&p3, p);
                                ImVec2 curr2D;
                                float depth = 0.0f;
                                bool currValid = ProjectWorldPointToViewport(scene, p3, imgMin, imgMax, uv0, uv1, curr2D, &depth);
                                currValid = currValid && depth >= 0.0f && depth <= 1.0f;

                                if (prevValid && currValid)
                                {
                                    const float d2 = DistancePointToSegmentSq(mousePos, prev2D, curr2D);
                                    if (d2 < bestD2)
                                    {
                                        bestD2 = d2;
                                        hoveredAxis = i;
                                    }
                                }

                                prev2D = curr2D;
                                prevValid = currValid;
                            }
                        }
                        if (bestD2 > 100.0f)
                        {
                            hoveredAxis = -1;
                        }
                    }
                    else if (g_ViewportGizmoMode == ViewportGizmoMode::Scale)
                    {
                        float bestHandleD2 = FLT_MAX;
                        for (int i = 0; i < 3; ++i)
                        {
                            const float dx = mousePos.x - gizmoAxisEnd2D[i].x;
                            const float dy = mousePos.y - gizmoAxisEnd2D[i].y;
                            const float d2 = dx * dx + dy * dy;
                            if (d2 < bestHandleD2)
                            {
                                bestHandleD2 = d2;
                                hoveredAxis = i;
                            }
                        }
                        if (bestHandleD2 > (scaleBoxHalf + 4.0f) * (scaleBoxHalf + 4.0f))
                        {
                            float bestD2 = FLT_MAX;
                            for (int i = 0; i < 3; ++i)
                            {
                                const float d2 = DistancePointToSegmentSq(mousePos, gizmoPivot2D, gizmoAxisEnd2D[i]);
                                if (d2 < bestD2)
                                {
                                    bestD2 = d2;
                                    hoveredAxis = i;
                                }
                            }
                            if (bestD2 > 100.0f)
                            {
                                hoveredAxis = -1;
                            }
                        }
                    }
                    else
                    {
                        float bestD2 = FLT_MAX;
                        for (int i = 0; i < 3; ++i)
                        {
                            const float d2 = DistancePointToSegmentSq(mousePos, gizmoPivot2D, gizmoAxisEnd2D[i]);
                            if (d2 < bestD2)
                            {
                                bestD2 = d2;
                                hoveredAxis = i;
                            }
                        }
                        if (bestD2 > 100.0f)
                        {
                            hoveredAxis = -1;
                        }
                    }
                }

                if (g_ViewportGizmoMode == ViewportGizmoMode::Rotate)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        const bool isHot = (i == hoveredAxis) || (i == s_ActiveAxis);
                        XMVECTOR ringU;
                        XMVECTOR ringV;
                        BuildRotateRingBasis(i, ringU, ringV);

                        ImVec2 prev2D(0.0f, 0.0f);
                        bool prevValid = false;
                        for (int s = 0; s <= ringSegments; ++s)
                        {
                            const float a = (6.28318530718f * (float)s) / (float)ringSegments;
                            const XMVECTOR p = XMVectorAdd(pivot, XMVectorAdd(
                                XMVectorScale(ringU, std::cos(a) * rotateRadiusWorld),
                                XMVectorScale(ringV, std::sin(a) * rotateRadiusWorld)));

                            XMFLOAT3 p3;
                            XMStoreFloat3(&p3, p);
                            ImVec2 curr2D;
                            float depth = 0.0f;
                            bool currValid = ProjectWorldPointToViewport(scene, p3, imgMin, imgMax, uv0, uv1, curr2D, &depth);
                            currValid = currValid && depth >= 0.0f && depth <= 1.0f;

                            if (prevValid && currValid)
                            {
                                drawList->AddLine(prev2D, curr2D, axisColors[i], isHot ? 3.5f : 2.0f);
                            }

                            prev2D = curr2D;
                            prevValid = currValid;
                        }
                    }
                }
                else if (g_ViewportGizmoMode == ViewportGizmoMode::Scale)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        const bool isHot = (i == hoveredAxis) || (i == s_ActiveAxis);
                        drawList->AddLine(gizmoPivot2D, gizmoAxisEnd2D[i], axisColors[i], isHot ? 3.4f : 2.2f);
                        drawList->AddRectFilled(
                            ImVec2(gizmoAxisEnd2D[i].x - scaleBoxHalf, gizmoAxisEnd2D[i].y - scaleBoxHalf),
                            ImVec2(gizmoAxisEnd2D[i].x + scaleBoxHalf, gizmoAxisEnd2D[i].y + scaleBoxHalf),
                            axisColors[i]);
                    }
                    drawList->AddRectFilled(
                        ImVec2(gizmoPivot2D.x - 4.0f, gizmoPivot2D.y - 4.0f),
                        ImVec2(gizmoPivot2D.x + 4.0f, gizmoPivot2D.y + 4.0f),
                        IM_COL32(235, 235, 235, 255));
                }
                else
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        const bool isHot = (i == hoveredAxis) || (i == s_ActiveAxis);
                        const float thickness = isHot ? 4.0f : 2.4f;
                        drawList->AddLine(gizmoPivot2D, gizmoAxisEnd2D[i], axisColors[i], thickness);

                        const ImVec2 dir = ImVec2(gizmoAxisEnd2D[i].x - gizmoPivot2D.x, gizmoAxisEnd2D[i].y - gizmoPivot2D.y);
                        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 1e-4f)
                        {
                            const ImVec2 n = ImVec2(dir.x / len, dir.y / len);
                            const ImVec2 p = ImVec2(-n.y, n.x);
                            const ImVec2 tip = gizmoAxisEnd2D[i];
                            const ImVec2 base = ImVec2(tip.x - n.x * 11.0f, tip.y - n.y * 11.0f);
                            drawList->AddTriangleFilled(
                                tip,
                                ImVec2(base.x + p.x * 4.0f, base.y + p.y * 4.0f),
                                ImVec2(base.x - p.x * 4.0f, base.y - p.y * 4.0f),
                                axisColors[i]);
                        }
                    }
                }
                drawList->AddCircleFilled(gizmoPivot2D, 4.0f, IM_COL32(240, 240, 240, 255), 12);

            }
        }
    }

    if (s_IsDraggingAxis && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        s_IsDraggingAxis = false;
        s_ActiveAxis = -1;
        s_DraggingActorKey = 0;
    }

    if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        bool clickConsumedByGizmo = false;
        if (hasValidGizmo && selectedRoot && hoveredAxis >= 0)
        {
            s_IsDraggingAxis = true;
            s_ActiveAxis = hoveredAxis;
            s_LastMousePos = mousePos;
            s_DraggingActorKey = selectedActor ? (uintptr_t)selectedActor.get() : 0;
            clickConsumedByGizmo = true;
        }

        if (!clickConsumedByGizmo)
        {
            Actor* prevSelected = nullptr;
            if (std::shared_ptr<Actor> prev = GEditor.selection.SelectedActor.lock())
            {
                prevSelected = prev.get();
            }

            float uTex = 0.0f;
            float vTex = 0.0f;
            ScreenPosToTextureUV(mousePos, imgMin, imgMax, uv0, uv1, uTex, vTex);

            DirectX::XMVECTOR rayOrigin;
            DirectX::XMVECTOR rayDir;
            if (BuildPickingRayFromTextureUV(scene, uTex, vTex, rayOrigin, rayDir))
            {
                GEditor.selection.SelectedActor = PickActorByRay(GEditor.world, rayOrigin, rayDir);

                Actor* currSelected = nullptr;
                if (std::shared_ptr<Actor> curr = GEditor.selection.SelectedActor.lock())
                {
                    currSelected = curr.get();
                }
                if (currSelected != prevSelected)
                {
                    RunRuntimeValidationForCurrentSelectionIfCharacter();
                }
            }
        }
    }

    if (s_IsDraggingAxis && selectedRoot && selectedActor && s_DraggingActorKey == (uintptr_t)selectedActor.get() && s_ActiveAxis >= 0 && s_ActiveAxis < 3)
    {
        bool changed = false;
        Transform& t = selectedRoot->GetRelativeTransform();

        if (g_ViewportGizmoMode == ViewportGizmoMode::Rotate)
        {
            using namespace DirectX;
            float uPrev = 0.0f, vPrev = 0.0f;
            float uCurr = 0.0f, vCurr = 0.0f;
            ScreenPosToTextureUV(s_LastMousePos, imgMin, imgMax, uv0, uv1, uPrev, vPrev);
            ScreenPosToTextureUV(mousePos, imgMin, imgMax, uv0, uv1, uCurr, vCurr);

            XMVECTOR prevRayO, prevRayD, currRayO, currRayD;
            if (BuildPickingRayFromTextureUV(scene, uPrev, vPrev, prevRayO, prevRayD) &&
                BuildPickingRayFromTextureUV(scene, uCurr, vCurr, currRayO, currRayD))
            {
                const XMMATRIX w = selectedRoot->GetWorldMatrix();
                const XMVECTOR pivotW = w.r[3];
                const XMVECTOR n = XMVector3Normalize(gizmoAxisWorld[s_ActiveAxis]);

                XMVECTOR prevHit, currHit;
                if (IntersectRayPlane(prevRayO, prevRayD, pivotW, n, prevHit) &&
                    IntersectRayPlane(currRayO, currRayD, pivotW, n, currHit))
                {
                    const XMVECTOR v0 = XMVector3Normalize(XMVectorSubtract(prevHit, pivotW));
                    const XMVECTOR v1 = XMVector3Normalize(XMVectorSubtract(currHit, pivotW));
                    if (XMVectorGetX(XMVector3LengthSq(v0)) > 1e-6f && XMVectorGetX(XMVector3LengthSq(v1)) > 1e-6f)
                    {
                        const float sinA = XMVectorGetX(XMVector3Dot(XMVector3Cross(v0, v1), n));
                        const float cosA = XMVectorGetX(XMVector3Dot(v0, v1));
                        const float deltaA = std::atan2(sinA, cosA);

                        float pitch = t.m_Rot.Pitch;
                        float yaw = t.m_Rot.Yaw;
                        float roll = t.m_Rot.Roll;
                        if (s_ActiveAxis == 0) pitch += deltaA;
                        if (s_ActiveAxis == 1) yaw += deltaA;
                        if (s_ActiveAxis == 2) roll += deltaA;
                        t.SetRotationEuler(pitch, yaw, roll);
                        changed = true;
                    }
                }
            }
        }
        else
        {
            const ImVec2 delta = ImVec2(mousePos.x - s_LastMousePos.x, mousePos.y - s_LastMousePos.y);
            const ImVec2 axis2D = ImVec2(
                gizmoAxisEnd2D[s_ActiveAxis].x - gizmoPivot2D.x,
                gizmoAxisEnd2D[s_ActiveAxis].y - gizmoPivot2D.y);
            const float axisLen = std::sqrt(axis2D.x * axis2D.x + axis2D.y * axis2D.y);
            if (axisLen > 1e-3f)
            {
                const ImVec2 axisDir2D = ImVec2(axis2D.x / axisLen, axis2D.y / axisLen);
                const float pixelDelta = delta.x * axisDir2D.x + delta.y * axisDir2D.y;

                if (g_ViewportGizmoMode == ViewportGizmoMode::Translate)
                {
                    using namespace DirectX;
                    const float movePerPixel = cameraDistance * 0.0026f;
                    XMVECTOR p = XMLoadFloat3(&t.m_Pos);
                    p = XMVectorAdd(p, XMVectorScale(gizmoAxisWorld[s_ActiveAxis], pixelDelta * movePerPixel));
                    XMFLOAT3 outPos;
                    XMStoreFloat3(&outPos, p);
                    t.SetPosition(outPos.x, outPos.y, outPos.z);
                    changed = true;
                }
                else
                {
                    float sx = t.m_Scale.x;
                    float sy = t.m_Scale.y;
                    float sz = t.m_Scale.z;
                    const float scalePerPixel = 0.01f;
                    if (s_ActiveAxis == 0) sx += pixelDelta * scalePerPixel;
                    if (s_ActiveAxis == 1) sy += pixelDelta * scalePerPixel;
                    if (s_ActiveAxis == 2) sz += pixelDelta * scalePerPixel;
                    if (sx < 0.01f) sx = 0.01f;
                    if (sy < 0.01f) sy = 0.01f;
                    if (sz < 0.01f) sz = 0.01f;
                    t.SetScale(sx, sy, sz);
                    changed = true;
                }
            }
        }

        s_LastMousePos = mousePos;
        if (changed)
        {
            selectedRoot->MarkDirty();
            MarkLevelDirty("Level modified.");
        }
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
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.34f, 0.24f, 0.10f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.29f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.46f, 0.33f, 0.14f, 1.00f));
        }

        if (ImGui::Selectable(name, selected))
        {
            GEditor.selection.SelectedActor = WeakActor;
            RunRuntimeValidationForCurrentSelectionIfCharacter();
        }

        if (selected)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            drawList->AddRectFilled(
                ImVec2(itemMin.x, itemMin.y + 1.0f),
                ImVec2(itemMin.x + 3.0f, itemMax.y - 1.0f),
                IM_COL32(246, 164, 52, 255));

            ImGui::PopStyleColor(3);
        }
    }
}

void ImGuiHandler::RenderDetailsPanel()
{
    ImGui::Begin("Details");
    RenderDetailsPanelContents();
    ImGui::End();
}

void ImGuiHandler::RenderAssetPathRow(const char* label, const std::string& value, bool allowCopy)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();

    if (value.empty())
    {
        ImGui::TextDisabled("(none)");
        return;
    }

    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-90.0f);
    char buffer[1024]{};
    strncpy_s(buffer, value.c_str(), _TRUNCATE);
    ImGui::InputText("##value", buffer, IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_ReadOnly);
    if (allowCopy)
    {
        ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            ImGui::SetClipboardText(value.c_str());
        }
    }
    ImGui::PopID();
}

void ImGuiHandler::RenderSelectedAssetSummary()
{
    if (!GAssetRegistry || m_contentBrowserState.selected == 0)
    {
        return;
    }

    const AssetMeta* meta = GAssetRegistry->GetMeta(m_contentBrowserState.selected);
    if (!meta)
    {
        return;
    }

    const std::string assetName = m_contentBrowserPanel.GetNameFromVirtualPath(meta->virtualPath);
    if (ImGui::BeginChild("##SelectedAssetSummary", ImVec2(0.0f, 230.0f), true))
    {
        ImGui::TextUnformatted("Selected Asset");
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", AssetTypeToString(meta->type));
        ImGui::Text("Name: %s", assetName.c_str());
        ImGui::Text("Asset ID: %llu", static_cast<unsigned long long>(meta->id));

        RenderAssetPathRow("Virtual Path", meta->virtualPath, true);
        RenderAssetPathRow("Source Path", meta->sourcePath, !meta->sourcePath.empty());
        RenderAssetPathRow("Artifact Path", meta->artifactPath, !meta->artifactPath.empty());

        ImGui::Text("Source Timestamp: %llu", static_cast<unsigned long long>(meta->sourceTimestamp));
        ImGui::Text("Source Size: %llu bytes", static_cast<unsigned long long>(meta->sourceSize));

        if (ImGui::Button("Rescan Asset"))
        {
            if (!meta->sourcePath.empty())
            {
                const std::filesystem::path sourcePath(meta->sourcePath);
                if (std::filesystem::exists(sourcePath))
                {
                    const_cast<AssetRegistry*>(GAssetRegistry)->UpsertFileFromPath(sourcePath);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Scan All"))
        {
            const_cast<AssetRegistry*>(GAssetRegistry)->ScanAll();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload DB"))
        {
            const_cast<AssetRegistry*>(GAssetRegistry)->LoadFromDisk();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save DB"))
        {
            const_cast<AssetRegistry*>(GAssetRegistry)->SaveToDisk();
        }

        if (meta->sourcePath.empty())
        {
            ImGui::TextDisabled("Source-oriented reload/reimport is unavailable for built-in or synthetic assets without a source path.");
        }
        else
        {
            ImGui::TextDisabled("Reimport currently reuses registry rescan/upsert for the selected source file. Material/mesh runtime override editing is still separate.");
        }

        if (!meta->tags.empty() && ImGui::TreeNode("Metadata Tags"))
        {
            for (const auto& kv : meta->tags)
            {
                ImGui::BulletText("%s = %s", kv.first.c_str(), kv.second.c_str());
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
}

void ImGuiHandler::RenderDetailsPanelContents()
{
    RenderSelectedAssetSummary();

    std::weak_ptr<Actor> a = GEditor.selection.SelectedActor;
    if (a.expired())
    {
        if (m_contentBrowserState.selected == 0)
        {
            ImGui::TextUnformatted("No selection.");
        }
        else
        {
            ImGui::Separator();
            ImGui::TextDisabled("No actor selected. Asset metadata above reflects the Content Browser selection.");
        }
        return;
    }

    if (m_contentBrowserState.selected != 0)
    {
        ImGui::Separator();
    }

    std::shared_ptr<Actor> actor = a.lock();
    const std::vector<std::shared_ptr<ActorComponent>>& comps = actor->GetComponents();
    const SceneComponent* rootComponent = actor->GetRootComponent();

    static uint32_t lastActorId = 0;
    static char actorNameBuf[256] = "";
    static char componentFilter[128] = "";

    if (lastActorId != actor->GetUniqueId())
    {
        lastActorId = actor->GetUniqueId();
        componentFilter[0] = '\0';
        strncpy_s(actorNameBuf, actor->GetName().c_str(), _TRUNCATE);
    }

    if (ImGui::BeginChild("##ActorSummary", ImVec2(0.0f, 70.0f), true))
    {
        ImGui::TextUnformatted("Actor");
        ImGui::SameLine();
        ImGui::TextDisabled("(ID: %u)", actor->GetUniqueId());
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##ActorName", actorNameBuf, IM_ARRAYSIZE(actorNameBuf)))
        {
            actor->SetName(actorNameBuf);
            MarkLevelDirty("Actor renamed.");
        }
        ImGui::TextDisabled("Components: %d", (int)comps.size());
    }
    ImGui::EndChild();

    if (Character* selectedCharacter = dynamic_cast<Character*>(actor.get()))
    {
        ImGui::Separator();
        ImGui::TextUnformatted("RootMotion Test Environment");
        ImGui::SetNextItemWidth(180.0f);
        ImGui::DragFloat("Pair Spacing X##rm_pair_spacing_x", &g_RootMotionPairSpacingX, 0.1f, 0.5f, 200.0f, "%.2f");
        if (ImGui::Button("Setup RootMotion ON/OFF Pair##setup_rootmotion_pair"))
        {
            SetupRootMotionPairForCurrentWorld();
        }
        ImGui::TextWrapped("%s", g_RootMotionPairSetupStatus.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted("RootMotion Runtime Validation");
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderInt("Frames##rm_runtime_frames", &g_RuntimeValidationFrames, 60, 3600);
        ImGui::Checkbox("Dump Frame/Bone CSV##rm_runtime_csv", &g_RuntimeValidationDumpCsv);

        if (ImGui::Button("Run ON/OFF Bone WorldTransform Validation##run_rm_runtime_validation"))
        {
            const int frames = (std::max)(60, g_RuntimeValidationFrames);
            const RootMotionRuntimeValidationOutput result =
                RunRootMotionRuntimeEquivalenceValidation(selectedCharacter, frames, g_RuntimeValidationDumpCsv);
            g_RuntimeValidationStatus = result.Summary;
        }

        const bool isPass = g_RuntimeValidationStatus.find(" PASS ") != std::string::npos;
        const bool isFail = g_RuntimeValidationStatus.find(" FAIL ") != std::string::npos;
        if (isPass)
        {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", g_RuntimeValidationStatus.c_str());
        }
        else if (isFail)
        {
            ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.35f, 1.0f), "%s", g_RuntimeValidationStatus.c_str());
        }
        else
        {
            ImGui::TextWrapped("%s", g_RuntimeValidationStatus.c_str());
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Components");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##details_component_filter", "Search components...", componentFilter, IM_ARRAYSIZE(componentFilter));
    ImGui::Separator();

    int visibleCount = 0;
    for (const std::shared_ptr<ActorComponent>& c : comps)
    {
        if (!c)
        {
            continue;
        }

        const bool isRoot = (dynamic_cast<SceneComponent*>(c.get()) == rootComponent);
        std::string display = c->GetName();
        if (display.empty() || display == "None")
        {
            display = c->GetTypeName();
        }

        std::string fullLabel = c->GetTypeName();
        fullLabel += " : ";
        fullLabel += display;
        if (isRoot)
        {
            fullLabel += " [Root]";
        }

        if (componentFilter[0] != '\0' && !ContainsCaseInsensitive(fullLabel, componentFilter))
        {
            continue;
        }

        ++visibleCount;
        const ImGuiTreeNodeFlags flags = isRoot ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        std::string header = fullLabel + "##comp_" + std::to_string((size_t)c.get());
        if (ImGui::CollapsingHeader(header.c_str(), flags))
        {
            RenderComponentDetails(c.get());
            ImGui::Separator();
        }
    }

    if (visibleCount == 0)
    {
        ImGui::TextDisabled("No components match filter.");
    }
}

void ImGuiHandler::RenderComponentDetails(ActorComponent* component)
{
    if (EditorReflection::DrawProperties(component))
    {
        MarkLevelDirty("Component property changed.");
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
        m_contentBrowserPanel.RenderContentBrowserPanel(*const_cast<AssetRegistry*>(GAssetRegistry), m_contentBrowserState);
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
                    MarkLevelDirty("Added static mesh actor from asset drop.");
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
                    MarkLevelDirty("Added skeletal mesh actor from asset drop.");
                }
            }
        }
    }
}

