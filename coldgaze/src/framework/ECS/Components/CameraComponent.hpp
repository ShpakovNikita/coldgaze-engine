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

	glm::vec3 rotation = glm::vec3();
	glm::vec3 cameraPos = glm::vec3();

	// TODO: remove device and memory
	CG::Vk::Device* vkDevice;
	CG::Vk::UniformBufferVS uniformBufferVS;

	float zoom = -2.5f;
	float fov = 60.0f;

	bool isActive = true;
};