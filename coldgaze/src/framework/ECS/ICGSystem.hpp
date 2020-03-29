#include "entt/entity/fwd.hpp"

union SDL_Event;

class ICGSystem
{
public:
	virtual ~ICGSystem() = default;

	virtual void Update(float deltaTime, entt::registry& registry) = 0;
	virtual void InputUpdate(float deltaTime, entt::registry& registry, const SDL_Event& event) = 0;
};