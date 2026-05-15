#include "ContentBrouwserPanel.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{
    static std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return (char)std::tolower(c);
        });
        return value;
    }

    std::string GetParentFolderPath(const std::string& path)
    {
        const size_t p = path.find_last_of('/');
        if (p == std::string::npos || p == 0)
        {
            return "/Game";
        }
        return path.substr(0, p);
    }

    bool IsVirtualRootPath(const std::string& path)
    {
        return path == "/Game" || path == "/Engine";
    }

    std::string GetRootForPath(const std::string& path)
    {
        if (path.rfind("/Engine", 0) == 0)
        {
            return "/Engine";
        }
        return "/Game";
    }

    std::string GetDefaultParentForPath(const std::string& path)
    {
        return GetRootForPath(path);
    }

    void AddFolderAndAncestors(const std::string& folder, std::unordered_set<std::string>& outFolders)
    {
        std::string current = folder.empty() ? "/Game" : folder;
        while (!current.empty())
        {
            outFolders.insert(current);
            if (IsVirtualRootPath(current))
            {
                break;
            }

            const std::string parent = GetParentFolderPath(current);
            if (parent == current)
            {
                break;
            }
            current = parent;
        }
    }

    std::string GetFolderDisplayName(const std::string& path)
    {
        if (path == "/Game")
        {
            return "Game";
        }
        if (path == "/Engine")
        {
            return "Engine";
        }

        const size_t p = path.find_last_of('/');
        if (p == std::string::npos)
        {
            return path;
        }
        return path.substr(p + 1);
    }

    bool IsSameOrAncestorPath(const std::string& maybeAncestor, const std::string& fullPath)
    {
        if (maybeAncestor == fullPath)
        {
            return true;
        }
        if (fullPath.size() <= maybeAncestor.size())
        {
            return false;
        }
        if (fullPath.rfind(maybeAncestor, 0) != 0)
        {
            return false;
        }
        return fullPath[maybeAncestor.size()] == '/';
    }

    void BuildFolderChildrenMap(const AssetRegistry& reg, std::unordered_map<std::string, std::vector<std::string>>& outChildren)
    {
        outChildren.clear();

        std::unordered_set<std::string> folders;
        folders.insert("/Game");
        folders.insert("/Engine");

        AssetQuery qAll;
        const std::vector<AssetID> all = reg.Query(qAll);
        for (AssetID id : all)
        {
            const AssetMeta* m = reg.GetMeta(id);
            if (!m)
            {
                continue;
            }

            const std::string folder = GetParentFolderPath(m->virtualPath);
            AddFolderAndAncestors(folder, folders);
        }

        for (const std::string& folder : folders)
        {
            if (IsVirtualRootPath(folder))
            {
                continue;
            }

            const std::string parent = GetParentFolderPath(folder);
            outChildren[parent].push_back(folder);
        }

        for (auto& kv : outChildren)
        {
            std::vector<std::string>& children = kv.second;
            std::sort(children.begin(), children.end(), [](const std::string& a, const std::string& b)
            {
                return GetFolderDisplayName(a) < GetFolderDisplayName(b);
            });
            children.erase(std::unique(children.begin(), children.end()), children.end());
        }
    }

    void RenderFolderTreeNode(
        const std::string& folderPath,
        const std::unordered_map<std::string, std::vector<std::string>>& childrenMap,
        ContentBrowserState& st)
    {
        const auto it = childrenMap.find(folderPath);
        const bool hasChildren = (it != childrenMap.end()) && !it->second.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (st.currentFolder == folderPath)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (IsSameOrAncestorPath(folderPath, st.currentFolder))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }

        const bool opened = ImGui::TreeNodeEx(folderPath.c_str(), flags, "%s", GetFolderDisplayName(folderPath).c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            st.currentFolder = folderPath;
        }

        if (!hasChildren || !opened)
        {
            return;
        }

        for (const std::string& childPath : it->second)
        {
            RenderFolderTreeNode(childPath, childrenMap, st);
        }

        ImGui::TreePop();
    }

    bool ShouldIncludeInCurrentFolder(const std::string& folder, const AssetMeta& meta, bool recursiveView)
    {
        if (folder.empty())
        {
            return true;
        }

        if (recursiveView)
        {
            return meta.virtualPath.rfind(folder, 0) == 0;
        }

        return GetParentFolderPath(meta.virtualPath) == folder;
    }
}

std::string ContentBrowserPanel::GetNameFromVirtualPath(const std::string& vp)
{
    size_t p = vp.find_last_of('/');
    if (p == std::string::npos) return vp;
    return vp.substr(p + 1);
}

void ContentBrowserPanel::CollectChildFolders(const AssetRegistry& reg, const std::string& parent, std::vector<std::string>& outFolders)
{
    outFolders.clear();
    std::unordered_map<std::string, bool> uniq;

    std::string prefix = parent;
    if (!prefix.empty() && prefix.back() != '/')
    {
        prefix.push_back('/');
    }

    AssetQuery q;
    q.virtualPathPrefix = parent;
    auto all = reg.Query(q);

    for (AssetID id : all)
    {
        const AssetMeta* m = reg.GetMeta(id);
        if (!m)
        {
            continue;
        }

        if (m->virtualPath.rfind(prefix, 0) != 0)
        {
            continue;
        }

        std::string rest = m->virtualPath.substr(prefix.size());
        size_t slash = rest.find('/');
        if (slash == std::string::npos)
        {
            continue;
        }

        std::string folder = rest.substr(0, slash);
        if (!folder.empty())
        {
            uniq[folder] = true;
        }
    }

    for (auto& kv : uniq)
    {
        outFolders.push_back(kv.first);
    }

    std::sort(outFolders.begin(), outFolders.end());
}

void ContentBrowserPanel::RenderContentBrowserPanel(AssetRegistry& reg, ContentBrowserState& st)
{
    ImGui::Begin("Content Browser");
    ImGui::Text("Assets: %d", (int)reg.Query(AssetQuery{}).size());
    ImGui::SameLine();
    ImGui::TextDisabled("Root: %s", st.currentFolder.c_str());

    if (ImGui::Button("Scan"))
    {
        reg.ScanAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("SaveDB"))
    {
        reg.SaveToDisk();
    }
    ImGui::SameLine();
    if (ImGui::Button("LoadDB"))
    {
        reg.LoadFromDisk();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Absolute Paths", &st.showAbsolutePaths);
    ImGui::SameLine();
    ImGui::Checkbox("Tile Metadata", &st.showAssetMetadataInBrowser);

    if (ImGui::TreeNode("Registry + Physical Roots"))
    {
        ImGui::Text("Database: %s", reg.GetDatabasePath().string().c_str());
        ImGui::Text("Content Root: %s", reg.GetContentRoot().string().c_str());
        ImGui::TextDisabled("Default browser presentation now prefers virtual paths. Absolute paths stay behind this disclosure.");
        ImGui::TreePop();
    }

    ImGui::Separator();
    if (ImGui::Button("Up"))
    {
        if (!IsVirtualRootPath(st.currentFolder))
        {
            size_t p = st.currentFolder.find_last_of('/');
            if (p != std::string::npos && p > 0)
            {
                st.currentFolder = st.currentFolder.substr(0, p);
            }
            else
            {
                st.currentFolder = GetDefaultParentForPath(st.currentFolder);
            }
        }
    }
    ImGui::SameLine();
    ImGui::Text("Virtual Path: %s", st.currentFolder.c_str());

    char buf[256]{};
    strncpy_s(buf, st.search.c_str(), _TRUNCATE);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputTextWithHint("##search", "Search by asset name or virtual path...", buf, IM_ARRAYSIZE(buf)))
    {
        st.search = buf;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Recursive", &st.recursiveView);

    ImGui::Separator();

    float leftW = 260.0f;
    ImGui::BeginChild("##Folders", ImVec2(leftW, 0), true);

    std::unordered_map<std::string, std::vector<std::string>> folderChildren;
    BuildFolderChildrenMap(reg, folderChildren);
    RenderFolderTreeNode("/Game", folderChildren, st);
    RenderFolderTreeNode("/Engine", folderChildren, st);

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##Tiles", ImVec2(0, 0), true);

    std::vector<AssetID> items;
    if (st.recursiveView)
    {
        AssetQuery q;
        q.virtualPathPrefix = st.currentFolder;
        items = reg.Query(q);
    }
    else
    {
        items = reg.ListDirectChildren(st.currentFolder);
    }

    items.erase(std::remove_if(items.begin(), items.end(), [&](AssetID id)
    {
        const AssetMeta* m = reg.GetMeta(id);
        if (!m)
        {
            return true;
        }
        if (!ShouldIncludeInCurrentFolder(st.currentFolder, *m, st.recursiveView))
        {
            return true;
        }

        if (!st.search.empty())
        {
            const std::string name = ToLower(GetNameFromVirtualPath(m->virtualPath));
            const std::string virtualPath = ToLower(m->virtualPath);
            const std::string search = ToLower(st.search);
            if (name.find(search) == std::string::npos && virtualPath.find(search) == std::string::npos)
            {
                return true;
            }
        }

        return false;
    }), items.end());

    std::sort(items.begin(), items.end(), [&](AssetID a, AssetID b)
    {
        const AssetMeta* A = reg.GetMeta(a);
        const AssetMeta* B = reg.GetMeta(b);
        if (!A || !B)
        {
            return a < b;
        }
        return A->virtualPath < B->virtualPath;
    });
    items.erase(std::unique(items.begin(), items.end()), items.end());

    ImGuiStyle& style = ImGui::GetStyle();
    const float tileW = 110.0f;
    const float tileH = 90.0f;
    const float pad = style.ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    int cols = (int)((avail + pad) / (tileW + pad));
    if (cols < 1)
    {
        cols = 1;
    }

    int col = 0;
    for (AssetID id : items)
    {
        const AssetMeta* m = reg.GetMeta(id);
        if (!m)
        {
            continue;
        }

        const bool selected = (st.selected == id);
        const std::string name = GetNameFromVirtualPath(m->virtualPath);

        const AssetThumbnailInfo& thumb = m_ThumbnailCache.GetOrCreate(*m);

        ImGui::BeginGroup();

        std::string btnLabel = thumb.badge + "##tile_" + std::to_string((uint64_t)id);
        ImVec4 base = thumb.tint;
        if (selected)
        {
            base.x = (base.x + 0.15f > 1.0f) ? 1.0f : (base.x + 0.15f);
            base.y = (base.y + 0.15f > 1.0f) ? 1.0f : (base.y + 0.15f);
            base.z = (base.z + 0.15f > 1.0f) ? 1.0f : (base.z + 0.15f);
        }

        const ImVec4 hovered(
            (base.x + 0.08f > 1.0f) ? 1.0f : (base.x + 0.08f),
            (base.y + 0.08f > 1.0f) ? 1.0f : (base.y + 0.08f),
            (base.z + 0.08f > 1.0f) ? 1.0f : (base.z + 0.08f),
            1.0f);
        const ImVec4 active(
            (base.x + 0.04f > 1.0f) ? 1.0f : (base.x + 0.04f),
            (base.y + 0.04f > 1.0f) ? 1.0f : (base.y + 0.04f),
            (base.z + 0.04f > 1.0f) ? 1.0f : (base.z + 0.04f),
            1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);

        if (thumb.previewSRV)
        {
            ImGui::Image((ImTextureID)thumb.previewSRV.Get(), ImVec2(tileW, tileH));

            const ImVec2 pmin = ImGui::GetItemRectMin();
            const ImVec2 pmax = ImGui::GetItemRectMax();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(pmin, ImVec2(pmin.x + 34.0f, pmin.y + 18.0f), IM_COL32(0, 0, 0, 170), 2.0f);
            draw->AddText(ImVec2(pmin.x + 4.0f, pmin.y + 2.0f), IM_COL32(230, 230, 230, 255), thumb.badge.c_str());
            if (selected)
            {
                draw->AddRect(pmin, pmax, IM_COL32(255, 220, 120, 255), 0.0f, 0, 2.0f);
            }
        }
        else
        {
            ImGui::Button(btnLabel.c_str(), ImVec2(tileW, tileH));
        }

        ImGui::PopStyleColor(3);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            st.selected = id;
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            AssetID payload = id;
            ImGui::SetDragDropPayload("ASSET_ID", &payload, sizeof(payload));
            ImGui::Text("Asset: %s", name.c_str());
            ImGui::TextDisabled("%s", m->virtualPath.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tileW);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TextDisabled("%s", AssetTypeToString(m->type));
        if (st.showAssetMetadataInBrowser)
        {
            ImGui::TextDisabled("%s", m->virtualPath.c_str());
            if (st.showAbsolutePaths && !m->sourcePath.empty())
            {
                ImGui::TextDisabled("src: %s", std::filesystem::path(m->sourcePath).filename().string().c_str());
            }
        }
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();

        col++;
        if (col < cols)
        {
            ImGui::SameLine();
        }
        else
        {
            col = 0;
        }
    }

    m_ThumbnailCache.GarbageCollect(items);

    ImGui::EndChild();
    ImGui::End();
}
