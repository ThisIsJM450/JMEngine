#pragma once
#include <memory>
class Actor;
struct EditorSelection
{
    std::weak_ptr<Actor> SelectedActor;
};

struct EditorViewportInputState
{
    bool bHovered = false;
    bool bFocused = false;
    bool bMouseCaptured = false;

    bool IsCameraInputEnabled() const
    {
        return bMouseCaptured || bHovered || bFocused;
    }
};

#include <string>
#include <vector>
#include <algorithm>

struct EditorDocumentState
{
    std::string currentLevelPath;
    std::string currentLevelName = "SavedLevel";
    std::string statusMessage;
    std::vector<std::string> recentLevelPaths;
    bool bLevelDirty = false;

    void MarkDirty()
    {
        bLevelDirty = true;
    }

    void TouchRecentLevel(const std::string& path)
    {
        if (path.empty())
        {
            return;
        }

        recentLevelPaths.erase(std::remove(recentLevelPaths.begin(), recentLevelPaths.end(), path), recentLevelPaths.end());
        recentLevelPaths.insert(recentLevelPaths.begin(), path);
        if (recentLevelPaths.size() > 8)
        {
            recentLevelPaths.resize(8);
        }
    }

    void ResetToNewLevel(const std::string& levelName = "NewLevel")
    {
        currentLevelPath.clear();
        currentLevelName = levelName.empty() ? "NewLevel" : levelName;
        bLevelDirty = false;
    }

    void MarkSaved(const std::string& path, const std::string& levelName)
    {
        currentLevelPath = path;
        if (!levelName.empty())
        {
            currentLevelName = levelName;
        }
        TouchRecentLevel(path);
        bLevelDirty = false;
    }
};

struct EditorContext
{
    class World* world = nullptr;
    class Renderer* renderer = nullptr;
    EditorSelection selection;
    EditorViewportInputState viewportInput;
    EditorDocumentState document;
};

extern EditorContext GEditor;  // 선언