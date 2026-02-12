#pragma once
#include "../Core/Asset/AssetRegistry.h"
#include <imgui.h>
#include <string>
#include <vector>

struct ContentBrowserState
{
    std::string currentFolder = "/Game"; // 현재 표시 폴더
    std::string search;
    AssetID selected = 0;
    bool recursiveView = true;
};

class ContentBrowserPanel
{
public:
    std::string GetNameFromVirtualPath(const std::string& vp);
    void CollectChildFolders(const AssetRegistry& reg, const std::string& parent, std::vector<std::string>& outFolders);
    void RenderContentBrowserPanel(AssetRegistry& reg, ContentBrowserState& st);
    
};
