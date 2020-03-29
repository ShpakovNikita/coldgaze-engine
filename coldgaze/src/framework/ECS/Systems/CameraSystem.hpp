#include "ECS/ICGSystem.hpp"

struct CameraComponent;

class CameraSystem
	: public ICGSystem
{
public:
	void Update(float deltaTime, entt::registry& registry) override;

	virtual ~CameraSystem() = default;

private:
	static void UpdateCameraData(CameraComponent& cameraComponent);
	static void UpdateUniformBuffers(CameraComponent& cameraComponent);
};