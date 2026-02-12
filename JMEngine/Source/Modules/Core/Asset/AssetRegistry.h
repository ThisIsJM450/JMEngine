#pragma once
#include "AssetType.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

struct AssetMeta
{
    AssetID id = 0;
    AssetType type = AssetType::Unknown;

    // 표시/조회용 (언리얼 느낌)
    std::string virtualPath;   // "/Game/Props/Crate"
    std::string sourcePath;    // "D:/Projects/JMEngine/Contents/Props/Crate.fbx"
    std::string artifactPath;  // "D:/Projects/JMEngine/Derived/StaticMesh/Crate.nxmesh" (선택)

    uint64_t sourceTimestamp = 0;
    uint64_t sourceSize = 0;

    std::unordered_map<std::string, std::string> tags;
    std::vector<AssetID> dependencies;
};

struct AssetQuery
{
    std::string virtualPathPrefix;  // "/Game/Props" 처럼 prefix 필터
    std::string text;               // 검색어(이름/경로)
    AssetType type = AssetType::Unknown; // Unknown이면 타입 필터 없음
};

class AssetRegistry
{
public:
    // 기본 ContentRoot: D:\Projects\JMEngine\Contents\
    AssetRegistry();

    AssetRegistry();
    // 설정
    void SetContentRoot(const std::filesystem::path& absPath);
    const std::filesystem::path& GetContentRoot() const { return m_ContentRoot; }

    // DB 파일 경로(기본: ContentRoot/../AssetRegistry.json 정도로 쓰고 싶으면 바꿔도 됨)
    void SetDatabasePath(const std::filesystem::path& absPathJson);
    const std::filesystem::path& GetDatabasePath() const { return m_DatabasePath; }

    // 스캔/로드/저장
    void ScanAll();                         // ContentRoot 전체 스캔
    void ScanDirectory(const std::filesystem::path& absDir); // 특정 폴더만(옵션)
    bool SaveToDisk() const;
    bool LoadFromDisk();

    // 조회
    const AssetMeta* GetMeta(AssetID id) const;
    AssetMeta* GetMeta(AssetID id);
    std::vector<AssetID> Query(const AssetQuery& q) const;

    // Content Browser 편의
    std::vector<AssetID> ListDirectChildren(const std::string& parentVirtualPath, AssetType typeFilter = AssetType::Unknown) const;

    // 변경/삭제 처리
    void RemoveBySourcePath(const std::string& absSourcePath);
    void Clear();

private:
    // 유틸
    static uint64_t ToUnixTimeSeconds(const std::filesystem::file_time_type& ft);
    static AssetType GuessTypeFromExtension(const std::filesystem::path& p);

    // virtualPath 규칙: ContentRoot 기준 상대경로를 "/Game/..."로
    std::string MakeVirtualPathFromAbsPath(const std::filesystem::path& absPath) const;

    // AssetID 생성: virtualPath + type로 안정적 해시
    static AssetID MakeAssetID(const std::string& virtualPath, AssetType type);

    // 스캔 내부
    void UpsertFileAsAsset(const std::filesystem::directory_entry& e);
    void GarbageCollectMissingFiles(const std::unordered_map<std::string, bool>& seen);

private:
    std::filesystem::path m_ContentRoot;   // 절대 경로
    std::filesystem::path m_DatabasePath;  // 절대 경로

    std::unordered_map<AssetID, AssetMeta> m_ById;
    std::unordered_map<std::string, AssetID> m_ByVirtualPath; // virtualPath -> id
    std::unordered_map<std::string, AssetID> m_BySourcePath;  // abs source path -> id
};
