#include "entt/entity/fwd.hpp"

class ICGSystem
{
public:
	virtual ~ICGSystem() = default;

	virtual void Update(float deltaTime, entt::registry& registry) = 0;
};