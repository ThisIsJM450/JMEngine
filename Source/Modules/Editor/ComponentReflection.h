#pragma once

#include <cstddef>
#include <cstdint>
#include <typeindex>
#include <vector>

class ActorComponent;

namespace EditorReflection
{
    struct PropertyDesc;

    enum class EPropertyType
    {
        Bool,
        Float,
        Float3,
        UInt64,
        Custom
    };

    using CustomPropertyDrawer = bool(*)(void* object, const PropertyDesc& desc);

    struct PropertyMeta
    {
        const char* DisplayName = "";
        const char* Category = "Default";
        bool Visible = true;
        bool Editable = true;
        bool HasRange = false;
        float Min = 0.0f;
        float Max = 0.0f;
        float Speed = 0.1f;
    };

    struct PropertyDesc
    {
        const char* Name = "";
        EPropertyType Type = EPropertyType::Float;
        size_t Offset = 0;
        PropertyMeta Meta{};
        CustomPropertyDrawer CustomDrawer = nullptr;
    };

    struct TypeDesc
    {
        std::type_index TypeId = typeid(void);
        const char* DisplayName = "";
        const TypeDesc* Parent = nullptr;
        std::vector<PropertyDesc> Properties;
    };

    const TypeDesc* FindType(const ActorComponent* component);
    bool DrawProperties(ActorComponent* component);
}
