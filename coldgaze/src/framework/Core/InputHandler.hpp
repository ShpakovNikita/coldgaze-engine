#include "SDL2/SDL_events.h"

namespace CG
{
	struct MouseInput
	{
		struct
		{
			float x = 0.0f;
			float y = 0.0f;
		} mousePos = {};

		bool left = false;
		bool right = false;
		bool middle = false;
	};

	class InputHandler
	{
	public:
		void AddEvent(const SDL_Event& event);
		void Reset();

		MouseInput mouseInput;

	private:
	};
}