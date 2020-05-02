#pragma once
#include <glm/glm.hpp>
#include "Render/Vulkan/UniformBufferVS.hpp"

namespace CG { namespace Vk { class Device; } }

struct CameraComponent
{
	struct ViewportParams
	{
		uint32_t width;
		uint32_t height;
	} viewport = {};

	struct CameraUniforms {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uboVS = {};

	struct Input
	{
		bool up = false;
		bool down = false;
		bool right = false;
		bool left = false;

		bool leftMouse = false;

		glm::vec2 newMousePos = { 0.0f, 0.0f };
		glm::vec2 oldMousePos = { 0.0f, 0.0f };

		inline bool IsRotating() const { return leftMouse; }
		inline bool IsMoving() const { return up || down || right || left; }
	} input = {};

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.5f);

	// TODO: remove device and memory
	CG::Vk::Device* vkDevice;
	CG::Vk::UniformBufferVS uniformBufferVS;

	float zoom = -2.5f;
	float fov = 60.0f;

	float movementSpeed = 0.5f;
	float sensitivity = 0.5f;

	bool isActive = true;

	void UpdateViewport(uint32_t width, uint32_t height);
};
