#pragma once

#include <memory>
#include <vector>

class InputSystem;
class World;

class IInputContext
{
public:
    virtual ~IInputContext() = default;
    virtual bool HandleInput(float deltaTime, const InputSystem& inputSystem, World& world) = 0;
};

class InputRouter
{
public:
    void AddContext(const std::shared_ptr<IInputContext>& context)
    {
        if (context)
        {
            m_Contexts.push_back(context);
        }
    }

    void Route(float deltaTime, const InputSystem& inputSystem, World& world)
    {
        for (const std::shared_ptr<IInputContext>& context : m_Contexts)
        {
            if (context && context->HandleInput(deltaTime, inputSystem, world))
            {
                return;
            }
        }
    }

private:
    std::vector<std::shared_ptr<IInputContext>> m_Contexts;
};
