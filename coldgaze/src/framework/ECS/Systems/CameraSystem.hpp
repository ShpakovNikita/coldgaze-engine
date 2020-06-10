#pragma once

#include "ECS/ICGSystem.hpp"
#include "entt/entity/fwd.hpp"

struct CameraComponent;
namespace CG {
namespace Vk {
    class Device;
}
}

class CameraSystem
    : public ICGSystem {
public:
    void Update(float deltaTime, entt::registry& registry) override;
    void InputUpdate(float deltaTime, entt::registry& registry, const SDL_Event& event) override;

    void SetDevice(const CG::Vk::Device* vkDevice);

    virtual ~CameraSystem() = default;

private:
    static void UpdateCameraFirstPerson(CameraComponent& cameraComponent, float deltaTime);
    static void UpdateCameraLookAt(CameraComponent& cameraComponent, float deltaTime);

    static void UpdateCameraView(CameraComponent& cameraComponent);
    static void UpdateMousePos(CameraComponent& cameraComponent, int32_t x, int32_t y);

    void UpdateUniformBuffers(CameraComponent& cameraComponent) const;

    entt::entity activeCameraEntity = {};

    const CG::Vk::Device* vkDevice = nullptr;
};
