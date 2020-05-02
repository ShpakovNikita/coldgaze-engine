#include "ECS/Systems/CameraSystem.hpp"
#include "ECS/Components/CameraComponent.hpp"
#include "entt/entity/registry.hpp"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/ext/matrix_transform.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Device.hpp"
#include "SDL2/SDL_events.h"
#include "imgui/imgui.h"

void CameraSystem::Update(float deltaTime, entt::registry& registry)
{
	registry.view<CameraComponent>().each([this, deltaTime](CameraComponent &cameraComponent)
		{
			if (cameraComponent.input.IsRotating())
			{
				UpdateRotation(cameraComponent, deltaTime);
			}

			if (cameraComponent.input.IsMoving())
			{
				UpdateCameraPosition(cameraComponent, deltaTime);
			}

			UpdateCameraView(cameraComponent);

			if (cameraComponent.isActive)
			{
				// UpdateUniformBuffers(cameraComponent);
			}
		}
	);
}

void CameraSystem::InputUpdate(float deltaTime, entt::registry& registry, const SDL_Event& event)
{
	CameraComponent& component = registry.get<CameraComponent>(activeCameraEntity);

	ImGuiIO& io = ImGui::GetIO();
	bool handled = !io.WantCaptureMouse;

	switch (event.type)
	{
	case SDL_MOUSEWHEEL:
	{
		if (handled)
		{
			switch (component.cameraType)
			{
			case CameraComponent::CameraType::LOOK_AT:
			{
				component.zoom = glm::clamp(component.zoom + event.wheel.y * deltaTime * 10.0f, 0.0f, 5.0f);
				component.position = glm::vec3(0.0f, 0.0f, -component.zoom);
			}
			break;
			case CameraComponent::CameraType::FIRST_PERSON:
			{
				float speedDelta = component.movementSpeed + event.wheel.y * deltaTime * 100.0f;
				component.movementSpeed = glm::clamp(speedDelta, 0.0f, 1.0f);
			}
			break;
			default:
				break;
			}
		}
	}
	break;

	case SDL_MOUSEMOTION:
	{
		if (handled)
		{
			UpdateMousePos(component, event.motion.x, event.motion.y);
		}
	}
	break;

	case SDL_MOUSEBUTTONDOWN:
	{
		if (handled)
		{
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				component.input.leftMouse = true;
				UpdateMousePos(component, event.button.x, event.button.y);
			default:
				break;
			}
		}
	}
	break;

	case SDL_MOUSEBUTTONUP:
	{
		if (handled)
		{
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				component.input.leftMouse = false;
				UpdateMousePos(component, event.button.x, event.button.y);
			default:
				break;
			}
		}
	}
	break;

	case SDL_KEYDOWN:
	{
		switch (event.key.keysym.sym)
		{
		case SDLK_w:
			component.input.up = true;
			break;
		case SDLK_a:
			component.input.left = true;
			break;
		case SDLK_s:
			component.input.down = true;
			break;
		case SDLK_d:
			component.input.right = true;
			break;
		}
	}
	break;

	case SDL_KEYUP:
	{
		switch (event.key.keysym.sym)
		{
		case SDLK_w:
			component.input.up = false;
			break;
		case SDLK_a:
			component.input.left = false;
			break;
		case SDLK_s:
			component.input.down = false;
			break;
		case SDLK_d:
			component.input.right = false;
			break;
		}
	}
	break;

	}
}

void CameraSystem::SetDevice(const CG::Vk::Device* aVkDevice)
{
	vkDevice = aVkDevice;
}

void CameraSystem::UpdateCameraView(CameraComponent& cameraComponent)
{
	CameraComponent::CameraUniforms& uboVS = cameraComponent.uboVS;
	uboVS.projectionMatrix = glm::perspective(glm::radians(cameraComponent.fov), 
		static_cast<float>(cameraComponent.viewport.width) / static_cast<float>(cameraComponent.viewport.height), 0.1f, 256.0f);

	glm::mat4 rotationMatrix = glm::mat4(1.0f);
	glm::mat4 translationMatrix;

	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cameraComponent.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cameraComponent.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cameraComponent.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	translationMatrix = glm::translate(glm::mat4(1.0f), cameraComponent.position);

	switch (cameraComponent.cameraType)
	{
	case CameraComponent::CameraType::LOOK_AT:
		uboVS.viewMatrix = translationMatrix * rotationMatrix;
		break;
	case CameraComponent::CameraType::FIRST_PERSON:
		uboVS.viewMatrix = rotationMatrix * translationMatrix;
		break;
	default:
		break;
	}
}

void CameraSystem::UpdateUniformBuffers(CameraComponent& cameraComponent) const
{
	CameraComponent::CameraUniforms& uboVS = cameraComponent.uboVS;

	cameraComponent.uniformBufferVS.buffer.Map(sizeof(uboVS));
	cameraComponent.uniformBufferVS.buffer.CopyTo(&uboVS, sizeof(uboVS));
	cameraComponent.uniformBufferVS.buffer.Unmap();
}

void CameraSystem::UpdateCameraPosition(CameraComponent& cameraComponent, float deltaTime)
{
	glm::vec3 camForwardVector;
	camForwardVector.x = -cos(cameraComponent.rotation.x) * sin(cameraComponent.rotation.y);
	camForwardVector.y = sin(cameraComponent.rotation.x);
	camForwardVector.z = cos(cameraComponent.rotation.x) * cos(cameraComponent.rotation.y);
	camForwardVector = glm::normalize(camForwardVector);

	float moveSpeed = deltaTime * cameraComponent.movementSpeed;

	if (cameraComponent.input.up)
		cameraComponent.position += camForwardVector * moveSpeed;
	if (cameraComponent.input.down)
		cameraComponent.position -= camForwardVector * moveSpeed;
	if (cameraComponent.input.left)
		cameraComponent.position -= glm::normalize(glm::cross(camForwardVector, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
	if (cameraComponent.input.right)
		cameraComponent.position += glm::normalize(glm::cross(camForwardVector, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
}

void CameraSystem::UpdateRotation(CameraComponent& cameraComponent, float deltaTime)
{
	float dx = cameraComponent.input.newMousePos.x - cameraComponent.input.oldMousePos.x;
	float dy = cameraComponent.input.newMousePos.y - cameraComponent.input.oldMousePos.y;

	cameraComponent.rotation += glm::vec3(dy * cameraComponent.sensitivity * deltaTime * 100.0f, -dx * cameraComponent.sensitivity * deltaTime * 100.0f, 0.0f);

	cameraComponent.input.oldMousePos.x = cameraComponent.input.newMousePos.x;
	cameraComponent.input.oldMousePos.y = cameraComponent.input.newMousePos.y;

}

void CameraSystem::UpdateMousePos(CameraComponent& cameraComponent, int32_t x, int32_t y)
{
	cameraComponent.input.oldMousePos.x = cameraComponent.input.newMousePos.x;
	cameraComponent.input.oldMousePos.y = cameraComponent.input.newMousePos.y;

	cameraComponent.input.newMousePos.x = static_cast<float>(x);
	cameraComponent.input.newMousePos.y = static_cast<float>(y);
}
