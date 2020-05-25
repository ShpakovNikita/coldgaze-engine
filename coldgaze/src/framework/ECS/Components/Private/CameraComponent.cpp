#pragma once
#include "ECS/Components/CameraComponent.hpp"
#include "glm/ext/matrix_clip_space.inl"

void CameraComponent::UpdateViewport(uint32_t width, uint32_t height)
{
	viewport.height = height;
	viewport.width = width;

	uboVS.projectionMatrix = glm::perspective(glm::radians(fov),
		static_cast<float>(viewport.width) / static_cast<float>(viewport.height), 0.001f, 256.0f);
}