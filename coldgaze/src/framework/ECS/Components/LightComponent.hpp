#pragma once
#include <glm/glm.hpp>
#include "Render/Vulkan/UniformBufferVS.hpp"

namespace CG { namespace Vk { class Device; } }

struct LightComponent
{
	struct CameraUniforms {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uboVS = {};

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.5f);

	CG::Vk::UniformBufferVS uniformBufferVS;

	float intensity = 0.0f;
	float radius = 0.0f;
};