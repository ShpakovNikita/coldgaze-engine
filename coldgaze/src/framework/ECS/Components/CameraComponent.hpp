#pragma once
#include "Render/Vulkan/UniformBufferVS.hpp"
#include <glm/glm.hpp>

namespace CG {
namespace Vk {
    class Device;
}
}

struct CameraComponent {
    enum class CameraType {
        kLookAt = 0,
        kFirstPerson,
    };

    struct ViewportParams {
        uint32_t width;
        uint32_t height;
    } viewport = {};

    struct CameraUniforms {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    } uboVS = {};

    struct Input {
        bool up = false;
        bool down = false;
        bool right = false;
        bool left = false;

        float wheelDelta = 0.0f;

        bool leftMouse = false;
        bool rightMouse = false;

        glm::vec2 newMousePos = { 0.0f, 0.0f };
        glm::vec2 oldMousePos = { 0.0f, 0.0f };

        inline bool IsRotating() const { return leftMouse; }
        inline bool IsMoving() const { return up || down || right || left || wheelDelta > 0.0f; }
    } input = {};

    void ResetSamples()
    {
        accumulationIndex = 0;
    }

    float zoom = 1.0f;

    glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 position = glm::vec3(0.0f, 0.0f, -zoom);

    CG::Vk::UniformBufferVS uniformBufferVS;

    float fov = 60.0f;

    float movementSpeed = 0.5f;
    float sensitivity = 0.5f;

    bool isActive = true;

    int accumulationIndex = 0;

    CameraType cameraType = CameraType::kFirstPerson;

    void UpdateViewport(uint32_t width, uint32_t height);
};
