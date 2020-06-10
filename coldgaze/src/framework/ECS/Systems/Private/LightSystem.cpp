#include "ECS/Systems/LightSystem.hpp"
#include "ECS/Components/LightComponent.hpp"
#include "entt/entity/registry.hpp"
#include "entt/entity/view.hpp"

void LightSystem::Update([[maybe_unused]] float deltaTime, [[maybe_unused]] entt::registry& registry)
{
    registry.view<LightComponent>().each([deltaTime]([[maybe_unused]] LightComponent& lightComponent) {

    });
}
