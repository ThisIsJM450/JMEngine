#pragma once
#include <memory>
class Actor;
struct EditorSelection
{
    std::weak_ptr<Actor> SelectedActor;
};

struct EditorContext
{
    class World* world = nullptr;
    class Renderer* renderer = nullptr;
    EditorSelection selection;
};

extern EditorContext GEditor;  // 선언