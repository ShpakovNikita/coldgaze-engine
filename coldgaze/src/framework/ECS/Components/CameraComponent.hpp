#pragma once
#include <glm/glm.hpp>
#include "Render/Vulkan/UniformBufferVS.hpp"

namespace CG { namespace Vk { class Device; } }

struct CameraComponent
{	
	enum class CameraType
	{
		LOOK_AT = 0,
		FIRST_PERSON,
	};

	struct ViewportParams
	{
		uint32_t width;
		uint32_t height;
	} viewport = {};

	struct CameraUniforms {
		glm::mat4 projectionMatrix;
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

	float zoom = 1.0f;

	glm::vec3 rotation = glm::vec3(180.0f, 0.0f, 0.0f);
	glm::vec3 position = glm::vec3(0.0f, 0.0f, -zoom);

	// TODO: remove device and memory
	CG::Vk::Device* vkDevice;
	CG::Vk::UniformBufferVS uniformBufferVS;

	float fov = 60.0f;

	float movementSpeed = 0.5f;
	float sensitivity = 0.5f;

	bool isActive = true;

	CameraType cameraType = CameraType::LOOK_AT;

	void UpdateViewport(uint32_t width, uint32_t height);
};
