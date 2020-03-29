#include "CameraSystem.hpp"
#include "ECS/Components/CameraComponent.hpp"
#include "entt/entity/registry.hpp"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/ext/matrix_transform.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Device.hpp"

void CameraSystem::Update(float deltaTime, entt::registry& registry)
{
	registry.view<CameraComponent>().each([deltaTime](CameraComponent &cameraComponent)
		{
			UpdateCameraData(cameraComponent);

			if (cameraComponent.isActive)
			{
				UpdateUniformBuffers(cameraComponent);
			}
		}
	);
}

void CameraSystem::UpdateCameraData(CameraComponent& cameraComponent)
{
	CameraComponent::CameraUniforms& uboVS = cameraComponent.uboVS;
	uboVS.projectionMatrix = glm::perspective(glm::radians(cameraComponent.fov), 
		static_cast<float>(cameraComponent.viewport.width) / static_cast<float>(cameraComponent.viewport.height), 0.1f, 256.0f);

	uboVS.viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, cameraComponent.zoom));

	// TODO: rotation matrix
	uboVS.modelMatrix = glm::mat4(1.0f);
	uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(cameraComponent.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(cameraComponent.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(cameraComponent.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
}

void CameraSystem::UpdateUniformBuffers(CameraComponent& cameraComponent)
{
	CameraComponent::CameraUniforms& uboVS = cameraComponent.uboVS;
	VkDevice device = cameraComponent.vkDevice->logicalDevice;

	uint8_t* pData;
	VK_CHECK_RESULT(vkMapMemory(device, cameraComponent.uniformBufferVS.memory, 0, sizeof(uboVS), 0, (void**)& pData));
	memcpy(pData, &uboVS, sizeof(uboVS));

	// Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU, that's why I can use unmap here
	vkUnmapMemory(device, cameraComponent.uniformBufferVS.memory);
}
