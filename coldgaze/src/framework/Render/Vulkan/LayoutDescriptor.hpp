#pragma once
#include <vector>

namespace CG {
namespace Vk {
    enum class eComponent {
        kPosition = 0,
        kNormal,
        kColor,
        kUv,
        kFloat,
        kVec4,
    };

    struct VertexLayout {
    public:
        std::vector<eComponent> components;

        VertexLayout(std::vector<eComponent> components)
        {
            this->components = std::move(components);
        }

        uint32_t Stride()
        {
            uint32_t res = 0;
            for (const auto& component : components) {
                switch (component) {
                case eComponent::kUv:
                    res += 2 * sizeof(float);
                    break;
                case eComponent::kFloat:
                    res += sizeof(float);
                    break;
                case eComponent::kVec4:
                    res += 4 * sizeof(float);
                    break;
                default:
                    res += 3 * sizeof(float);
                }
            }
            return res;
        }
    };
}
}
