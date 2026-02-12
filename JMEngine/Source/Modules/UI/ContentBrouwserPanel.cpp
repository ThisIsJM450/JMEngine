#include "ContentBrouwserPanel.h"

std::string ContentBrowserPanel::GetNameFromVirtualPath(const std::string& vp)
{
    size_t p = vp.find_last_of('/');
    if (p == std::string::npos) return vp;
    return vp.substr(p + 1);
}

// 폴더 목록은 "직접 폴더 엔트리"가 아니라, 아이템들의 경로를 잘라서 만듦(언리얼도 비슷한 개념)
void ContentBrowserPanel::CollectChildFolders(const AssetRegistry& reg, const std::string& parent, std::vector<std::string>& outFolders)
{
    outFolders.clear();
    std::unordered_map<std::string, bool> uniq;

    std::string prefix = parent;
    if (!prefix.empty() && prefix.back() != '/') prefix.push_back('/');

    // 전체 asset을 돌며 prefix 이후 첫 토큰을 폴더로 수집
    AssetQuery q;
    q.virtualPathPrefix = parent;
    auto all = reg.Query(q);

    for (AssetID id : all)
    {
        const AssetMeta* m = reg.GetMeta(id);
        if (!m) continue;

        if (m->virtualPath.rfind(prefix, 0) != 0)
            continue;

        std::string rest = m->virtualPath.substr(prefix.size());
        size_t slash = rest.find('/');
        if (slash == std::string::npos)
            continue; // direct file

        std::string folder = rest.substr(0, slash);
        if (!folder.empty())
            uniq[folder] = true;
    }

    for (auto& kv : uniq)
        outFolders.push_back(kv.first);

    std::sort(outFolders.begin(), outFolders.end());
}

void ContentBrowserPanel::RenderContentBrowserPanel(AssetRegistry& reg, ContentBrowserState& st)
{
    ImGui::Begin("Content Browser");
    ImGui::Text("Assets: %d", (int)reg.Query(AssetQuery{}).size());

    // 상단: ContentRoot 표시 + Scan/Save/Load
    ImGui::Text("ContentRoot: %s", reg.GetContentRoot().string().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Scan"))
        reg.ScanAll();
    ImGui::SameLine();
    if (ImGui::Button("SaveDB"))
        reg.SaveToDisk();
    ImGui::SameLine();
    if (ImGui::Button("LoadDB"))
        reg.LoadFromDisk();

    // 폴더 이동 (Up 버튼)
    ImGui::Separator();
    if (ImGui::Button("Up"))
    {
        if (st.currentFolder != "/Game")
        {
            size_t p = st.currentFolder.find_last_of('/');
            if (p != std::string::npos && p > 0)
                st.currentFolder = st.currentFolder.substr(0, p);
            else
                st.currentFolder = "/Game";
        }
    }
    ImGui::SameLine();
    ImGui::Text("Folder: %s", st.currentFolder.c_str());

    // 검색
    char buf[256]{};
    strncpy_s(buf, st.search.c_str(), _TRUNCATE);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputTextWithHint("##search", "Search...", buf, IM_ARRAYSIZE(buf)))
        st.search = buf;

    ImGui::Separator();

    // 좌: 폴더 목록 / 우: 타일
    float leftW = 260.0f;
    ImGui::BeginChild("##Folders", ImVec2(leftW, 0), true);

    std::vector<std::string> folders;
    CollectChildFolders(reg, st.currentFolder, folders);

    // 현재 폴더의 "자식 폴더" 나열
    for (const std::string& f : folders)
    {
        std::string full = st.currentFolder;
        if (!full.empty() && full.back() != '/') full.push_back('/');
        full += f;

        if (ImGui::Selectable(f.c_str(), false))
            st.currentFolder = full;
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##Tiles", ImVec2(0, 0), true);

    
    std::vector<AssetID> items;
    if (st.recursiveView)
    {
        AssetQuery q;
        q.virtualPathPrefix = st.currentFolder; // 현재 폴더 아래 전부
        items = reg.Query(q);
    }
    else
    {
        items = reg.ListDirectChildren(st.currentFolder); // 현재 폴더 direct children 파일들(깊이 1)
    }

    // 검색어 있으면 추가 필터(이 폴더 내에서만)
    if (!st.search.empty())
    {
        const std::string s = st.search;
        items.erase(std::remove_if(items.begin(), items.end(), [&](AssetID id)
        {
            const AssetMeta* m = reg.GetMeta(id);
            if (!m) return true;
            const std::string name = GetNameFromVirtualPath(m->virtualPath);
            return name.find(s) == std::string::npos;
        }), items.end());
    }

    // 타일 계산
    ImGuiStyle& style = ImGui::GetStyle();
    const float tileW = 110.0f;
    const float tileH = 90.0f;
    const float pad = style.ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    int cols = (int)((avail + pad) / (tileW + pad));
    if (cols < 1) cols = 1;

    int col = 0;
    for (AssetID id : items)
    {
        const AssetMeta* m = reg.GetMeta(id);
        if (!m) continue;

        const bool selected = (st.selected == id);
        const std::string name = GetNameFromVirtualPath(m->virtualPath);

        ImGui::BeginGroup();

        // 썸네일 MVP: 없음 -> 버튼으로 대체(나중에 타입별 아이콘 SRV 넣기)
        std::string btnId = "##tile_" + std::to_string((uint64_t)id);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

        ImGui::Button(btnId.c_str(), ImVec2(tileW, tileH));

        if (selected)
            ImGui::PopStyleColor();

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            st.selected = id;

        // 드래그&드롭 payload는 AssetID로
        if (ImGui::BeginDragDropSource())
        {
            AssetID payload = id;
            ImGui::SetDragDropPayload("ASSET_ID", &payload, sizeof(payload));
            ImGui::Text("Asset: %s", name.c_str());
            ImGui::EndDragDropSource();
        }

        // 이름 + 타입 표시
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tileW);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TextDisabled("%s", AssetTypeToString(m->type));
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();

        col++;
        if (col < cols) ImGui::SameLine();
        else col = 0;
    }

    ImGui::EndChild();

    ImGui::End();
}
