#pragma once

#include <string>

#include "AssetType.h"

class IAssetReferenceable
{
public:
    virtual ~IAssetReferenceable() = default;

    virtual void SetAssetIdentity(AssetID id, AssetType type, const std::string& virtualPath, const std::string& sourcePath = std::string()) = 0;
    virtual AssetID GetAssetID() const = 0;
    virtual AssetType GetAssetType() const = 0;
    virtual const std::string& GetAssetVirtualPath() const = 0;
    virtual const std::string& GetAssetSourcePath() const = 0;
    virtual bool HasAssetIdentity() const = 0;
};

class AssetReferenceableMixin : public IAssetReferenceable
{
public:
    void SetAssetIdentity(AssetID id, AssetType type, const std::string& virtualPath, const std::string& sourcePath = std::string()) override
    {
        m_AssetId = id;
        m_AssetType = type;
        m_AssetVirtualPath = virtualPath;
        m_AssetSourcePath = sourcePath;
    }

    AssetID GetAssetID() const override { return m_AssetId; }
    AssetType GetAssetType() const override { return m_AssetType; }
    const std::string& GetAssetVirtualPath() const override { return m_AssetVirtualPath; }
    const std::string& GetAssetSourcePath() const override { return m_AssetSourcePath; }
    bool HasAssetIdentity() const override { return m_AssetId != 0 || !m_AssetVirtualPath.empty(); }

private:
    AssetID m_AssetId = 0;
    AssetType m_AssetType = AssetType::Unknown;
    std::string m_AssetVirtualPath;
    std::string m_AssetSourcePath;
};
