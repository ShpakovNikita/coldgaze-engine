#include "ECS/ICGSystem.hpp"
#include "entt/entity/fwd.hpp"

struct CameraComponent;

class CameraSystem
	: public ICGSystem
{
public:
	void Update(float deltaTime, entt::registry& registry) override;
	void InputUpdate(float deltaTime, entt::registry& registry, const SDL_Event& event) override;

	virtual ~CameraSystem() = default;

private:
	static void UpdateCameraView(CameraComponent& cameraComponent);
	static void UpdateUniformBuffers(CameraComponent& cameraComponent);
	static void UpdateCameraPosition(CameraComponent& cameraComponent, float deltaTime);
	static void UpdateRotation(CameraComponent& cameraComponent, float deltaTime);
	static void UpdateMousePos(CameraComponent& cameraComponent, int32_t x, int32_t y);

	entt::entity activeCameraEntity;
};