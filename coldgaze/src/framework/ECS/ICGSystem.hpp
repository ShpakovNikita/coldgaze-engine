#pragma once

#include "entt/entity/fwd.hpp"

union SDL_Event;

class ICGSystem {
public:
    virtual ~ICGSystem() = default;

    virtual void Update(float deltaTime, entt::registry& registry) = 0;
    virtual void InputUpdate([[maybe_unused]] float deltaTime,
        [[maybe_unused]] entt::registry& registry, [[maybe_unused]] const SDL_Event& event) {};
};
