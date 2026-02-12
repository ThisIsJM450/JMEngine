#include "AssetRegistry.h"
#include <algorithm>
#include <fstream>

// nlohmann/json
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

static std::string ToLower(std::string s)
{
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

AssetRegistry::AssetRegistry()
{
    // 기본 루트
    m_ContentRoot = std::filesystem::path("D:\\Projects\\JMEngine\\Contents\\");
    // 기본 DB 위치(원하면 바꿔도 됨)
    m_DatabasePath = std::filesystem::path("D:\\Projects\\JMEngine\\AssetRegistry.json");
}

void AssetRegistry::SetContentRoot(const std::filesystem::path& absPath)
{
    m_ContentRoot = absPath;
}

void AssetRegistry::SetDatabasePath(const std::filesystem::path& absPathJson)
{
    m_DatabasePath = absPathJson;
}

void AssetRegistry::Clear()
{
    m_ById.clear();
    m_ByVirtualPath.clear();
    m_BySourcePath.clear();
}

const AssetMeta* AssetRegistry::GetMeta(AssetID id) const
{
    auto it = m_ById.find(id);
    return (it != m_ById.end()) ? &it->second : nullptr;
}

AssetMeta* AssetRegistry::GetMeta(AssetID id)
{
    auto it = m_ById.find(id);
    return (it != m_ById.end()) ? &it->second : nullptr;
}

uint64_t AssetRegistry::ToUnixTimeSeconds(const std::filesystem::file_time_type& ft)
{
    // file_time_type -> system_clock 변환(대부분 구현에서 가능)
    // 단순화: time_since_epoch를 초로
    auto s = std::chrono::time_point_cast<std::chrono::seconds>(ft).time_since_epoch().count();
    if (s < 0) s = 0;
    return (uint64_t)s;
}

AssetType AssetRegistry::GuessTypeFromExtension(const std::filesystem::path& p)
{
    const std::string ext = ToLower(p.extension().string());
    
    if (ext == ".obj" || ext == ".gltf") return AssetType::StaticMesh;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds") return AssetType::Texture2D;
    if (ext == ".mat") return AssetType::Material;
    if (ext == ".hlsl" || ext == ".fx") return AssetType::Shader;
    if (ext == ".scene" || ext == ".json") return AssetType::Scene;
    
    if (ext == ".fbx")
    {
        Assimp::Importer importer;

        // Probe는 가볍게: Triangulate 같은 후처리도 굳이 필요 없음
        unsigned flags = aiProcess_SortByPType; 
        const aiScene* scene = importer.ReadFile(p.string(), flags);
        if (!scene)
        {
            return AssetType::Unknown;
        }

        const bool hasMesh = scene->HasMeshes() && scene->mNumMeshes > 0;
        if (hasMesh)
        {
            bool hasSkinned = false;
            for (unsigned i = 0; i < scene->mNumMeshes; ++i)
            {
                const aiMesh* m = scene->mMeshes[i];
                if (m && m->HasBones() && m->mNumBones > 0)
                {
                    hasSkinned = true;
                    break;
                }
            }

            return hasSkinned ? AssetType::SkeletalMesh : AssetType::StaticMesh;
        }
        if (scene->HasAnimations() && scene->mNumAnimations > 0)
        {
            return AssetType::Animation;
        }
    }

    return AssetType::Unknown;
}

std::string AssetRegistry::MakeVirtualPathFromAbsPath(const std::filesystem::path& absPath) const
{
    // absPath가 contentroot 안에 있어야 함
    std::filesystem::path rel;
    try
    {
        rel = std::filesystem::relative(absPath, m_ContentRoot);
    }
    catch (...)
    {
        // contentroot 밖이면 그냥 파일명으로
        rel = absPath.filename();
    }

    // 확장자 제거
    rel.replace_extension("");

    // "\\", "/" -> "/"로
    std::string relStr = rel.generic_string(); // generic_string은 '/' 사용
    // "/Game/" prefix
    return std::string("/Game/") + relStr;
}

static uint64_t Fnv1a64(const void* data, size_t sz)
{
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < sz; i++)
    {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

AssetID AssetRegistry::MakeAssetID(const std::string& virtualPath, AssetType type)
{
    // 안정적 ID: (virtualPath + type) 해시
    const std::string key = virtualPath + "#" + std::to_string((uint32_t)type);
    return (AssetID)Fnv1a64(key.data(), key.size());
}

void AssetRegistry::UpsertFileAsAsset(const std::filesystem::directory_entry& e)
{
    if (!e.is_regular_file()) return;

    const std::filesystem::path absPath = e.path();
    AssetType t = GuessTypeFromExtension(absPath);
    
    if (t == AssetType::Unknown)
    {
        return; 
    }

    const std::string sourcePath = absPath.lexically_normal().string();
    const std::string vpath = MakeVirtualPathFromAbsPath(absPath);
    const AssetID id = MakeAssetID(vpath, t);

    // 파일 속성
    uint64_t ts = 0;
    uint64_t size = 0;
    try
    {
        ts = ToUnixTimeSeconds(e.last_write_time());
        size = (uint64_t)e.file_size();
    }
    catch (...) {}

    // 기존이 있으면 업데이트, 없으면 새로
    AssetMeta meta;
    meta.id = id;
    meta.type = t;
    meta.virtualPath = vpath;
    meta.sourcePath = sourcePath;
    meta.sourceTimestamp = ts;
    meta.sourceSize = size;

    // tags 기본값
    meta.tags["Ext"] = ToLower(absPath.extension().string());
    meta.tags["Name"] = absPath.stem().string();

    // artifactPath는 일단 비움(Importer 붙이면 채우기)
    // 예: Derived 폴더 정책을 정하면 여기에 기본 경로를 만들어둘 수 있음.

    // 기존 메타가 있으면 tags/dep 유지하고 파일 변경분만 갱신
    auto itOld = m_ById.find(id);
    if (itOld != m_ById.end())
    {
        AssetMeta& old = itOld->second;

        old.type = meta.type;
        old.virtualPath = meta.virtualPath;
        old.sourcePath = meta.sourcePath;
        old.sourceTimestamp = meta.sourceTimestamp;
        old.sourceSize = meta.sourceSize;

        // 기본 tag 업데이트
        old.tags["Ext"] = meta.tags["Ext"];
        old.tags["Name"] = meta.tags["Name"];

        // 매핑 갱신
        m_ByVirtualPath[old.virtualPath] = id;
        m_BySourcePath[old.sourcePath] = id;
        return;
    }

    // 신규
    m_ById[id] = meta;
    m_ByVirtualPath[vpath] = id;
    m_BySourcePath[sourcePath] = id;
}

void AssetRegistry::GarbageCollectMissingFiles(const std::unordered_map<std::string, bool>& seen)
{
    // sourcePath 기준으로 존재하지 않는 건 제거
    std::vector<AssetID> toRemove;
    toRemove.reserve(m_ById.size());

    for (auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;
        auto it = seen.find(m.sourcePath);
        if (it == seen.end())
            toRemove.push_back(m.id);
    }

    for (AssetID id : toRemove)
    {
        auto it = m_ById.find(id);
        if (it == m_ById.end()) continue;

        m_ByVirtualPath.erase(it->second.virtualPath);
        m_BySourcePath.erase(it->second.sourcePath);
        m_ById.erase(it);
    }
}

void AssetRegistry::ScanDirectory(const std::filesystem::path& absDir)
{
    if (!std::filesystem::exists(absDir))
        return;

    std::unordered_map<std::string, bool> seen;

    for (auto& e : std::filesystem::recursive_directory_iterator(absDir))
    {
        if (!e.is_regular_file()) continue;

        const std::string sp = e.path().lexically_normal().string();
        seen[sp] = true;

        UpsertFileAsAsset(e);
    }

    // 여기서는 absDir 부분만 스캔했지만, MVP에선 전체 GC는 ScanAll에서 수행하는 걸 권장
}

void AssetRegistry::ScanAll()
{
    if (!std::filesystem::exists(m_ContentRoot))
        return;

    std::unordered_map<std::string, bool> seen;

    for (auto& e : std::filesystem::recursive_directory_iterator(m_ContentRoot))
    {
        if (!e.is_regular_file()) continue;

        const std::string sp = e.path().lexically_normal().string();
        seen[sp] = true;

        UpsertFileAsAsset(e);
    }

    GarbageCollectMissingFiles(seen);
}

std::vector<AssetID> AssetRegistry::Query(const AssetQuery& q) const
{
    std::vector<AssetID> out;
    out.reserve(m_ById.size());

    const std::string text = ToLower(q.text);
    const std::string prefix = q.virtualPathPrefix;

    for (const auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;

        if (q.type != AssetType::Unknown && m.type != q.type)
            continue;

        if (!prefix.empty())
        {
            if (m.virtualPath.rfind(prefix, 0) != 0) // starts_with
                continue;
        }

        if (!text.empty())
        {
            const std::string vp = ToLower(m.virtualPath);
            const std::string name = ToLower(m.tags.count("Name") ? m.tags.at("Name") : "");
            if (vp.find(text) == std::string::npos && name.find(text) == std::string::npos)
                continue;
        }

        out.push_back(m.id);
    }

    // 정렬: virtualPath
    std::sort(out.begin(), out.end(), [&](AssetID a, AssetID b)
    {
        const AssetMeta* A = GetMeta(a);
        const AssetMeta* B = GetMeta(b);
        if (!A || !B) return a < b;
        return A->virtualPath < B->virtualPath;
    });

    return out;
}

std::vector<AssetID> AssetRegistry::ListDirectChildren(const std::string& parentVirtualPath, AssetType typeFilter) const
{
    // parentVirtualPath 바로 아래(1 depth)만 나열 (ContentBrowser에서 현재 폴더 표시용)
    // 예: parent="/Game/Props" -> "/Game/Props/Crate", "/Game/Props/Barrel" 등
    std::vector<AssetID> out;
    out.reserve(256);

    std::string prefix = parentVirtualPath;
    if (!prefix.empty() && prefix.back() != '/')
        prefix.push_back('/');

    for (const auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;

        if (typeFilter != AssetType::Unknown && m.type != typeFilter)
            continue;

        if (m.virtualPath.rfind(prefix, 0) != 0)
            continue;

        // 깊이 1 제한: prefix 이후에 '/'가 없어야 direct child
        const std::string rest = m.virtualPath.substr(prefix.size());
        if (rest.find('/') != std::string::npos)
            continue;

        out.push_back(m.id);
    }

    std::sort(out.begin(), out.end(), [&](AssetID a, AssetID b)
    {
        const AssetMeta* A = GetMeta(a);
        const AssetMeta* B = GetMeta(b);
        if (!A || !B) return a < b;
        return A->virtualPath < B->virtualPath;
    });

    return out;
}

void AssetRegistry::RemoveBySourcePath(const std::string& absSourcePath)
{
    auto itId = m_BySourcePath.find(absSourcePath);
    if (itId == m_BySourcePath.end())
        return;

    AssetID id = itId->second;
    auto it = m_ById.find(id);
    if (it == m_ById.end())
        return;

    m_ByVirtualPath.erase(it->second.virtualPath);
    m_BySourcePath.erase(it->second.sourcePath);
    m_ById.erase(it);
}

bool AssetRegistry::SaveToDisk() const
{
    try
    {
        json j;
        j["contentRoot"] = m_ContentRoot.string();
        j["assets"] = json::array();

        for (const auto& kv : m_ById)
        {
            const AssetMeta& m = kv.second;
            json a;
            a["id"] = m.id;
            a["type"] = std::string(AssetTypeToString(m.type));
            a["virtualPath"] = m.virtualPath;
            a["sourcePath"] = m.sourcePath;
            a["artifactPath"] = m.artifactPath;
            a["sourceTimestamp"] = m.sourceTimestamp;
            a["sourceSize"] = m.sourceSize;

            json tags = json::object();
            for (const auto& t : m.tags) tags[t.first] = t.second;
            a["tags"] = tags;

            json deps = json::array();
            for (AssetID d : m.dependencies) deps.push_back(d);
            a["dependencies"] = deps;

            j["assets"].push_back(a);
        }

        std::ofstream ofs(m_DatabasePath, std::ios::binary);
        if (!ofs) return false;

        ofs << j.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool AssetRegistry::LoadFromDisk()
{
    try
    {
        std::ifstream ifs(m_DatabasePath, std::ios::binary);
        if (!ifs) return false;

        json j;
        ifs >> j;

        Clear();

        if (j.contains("contentRoot"))
            m_ContentRoot = std::filesystem::path(j["contentRoot"].get<std::string>());

        if (!j.contains("assets"))
            return true;

        for (auto& a : j["assets"])
        {
            AssetMeta m;
            m.id = a.value("id", 0ull);
            m.type = AssetTypeFromString(a.value("type", "Unknown"));
            m.virtualPath = a.value("virtualPath", "");
            m.sourcePath = a.value("sourcePath", "");
            m.artifactPath = a.value("artifactPath", "");
            m.sourceTimestamp = a.value("sourceTimestamp", 0ull);
            m.sourceSize = a.value("sourceSize", 0ull);

            if (a.contains("tags"))
            {
                for (auto it = a["tags"].begin(); it != a["tags"].end(); ++it)
                    m.tags[it.key()] = it.value().get<std::string>();
            }

            if (a.contains("dependencies"))
            {
                for (auto& d : a["dependencies"])
                    m.dependencies.push_back(d.get<uint64_t>());
            }

            m_ById[m.id] = m;
            if (!m.virtualPath.empty()) m_ByVirtualPath[m.virtualPath] = m.id;
            if (!m.sourcePath.empty())  m_BySourcePath[m.sourcePath] = m.id;
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}
