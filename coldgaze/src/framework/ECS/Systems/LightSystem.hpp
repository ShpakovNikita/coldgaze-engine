#pragma once

#include "ECS/ICGSystem.hpp"
#include "entt/entity/fwd.hpp"

struct LightComponent;

class LightSystem
    : public ICGSystem {
public:
    void Update(float deltaTime, entt::registry& registry) override;

    virtual ~LightSystem() = default;
};
