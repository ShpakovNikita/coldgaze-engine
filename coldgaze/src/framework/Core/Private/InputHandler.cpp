#include "Core/InputHandler.hpp"

void CG::InputHandler::AddEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_MOUSEMOTION: {
        mouseInput.mousePos.x = static_cast<float>(event.motion.x);
        mouseInput.mousePos.y = static_cast<float>(event.motion.y);
    } break;

    case SDL_MOUSEBUTTONDOWN: {
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
            mouseInput.left = true;
        case SDL_BUTTON_MIDDLE:
            mouseInput.middle = true;
        case SDL_BUTTON_RIGHT:
            mouseInput.right = true;
        default:
            break;
        }
    } break;

    case SDL_MOUSEBUTTONUP: {
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
            mouseInput.left = false;
        case SDL_BUTTON_MIDDLE:
            mouseInput.middle = false;
        case SDL_BUTTON_RIGHT:
            mouseInput.right = false;
        default:
            break;
        }
    } break;
    }
}

void CG::InputHandler::Reset()
{
    // pass for now
}
