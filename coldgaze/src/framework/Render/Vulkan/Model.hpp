#pragma once
#include <vector>


namespace CG
{
	namespace Vk
	{
		// Layout structure
		enum class VertexComponent : uint8_t {
			POSITION = 0,
			NORMAL,
			COLOR,
			UV,
			TANGENT,
			BITANGENT,
			DUMMY_FLOAT,
			DUMMY_VEC4,
		};

		struct VertexLayout {
		public:
			/** @brief Components used to generate vertices from */
			std::vector<VertexComponent> components;

			VertexLayout(const std::vector<VertexComponent>& components)
			{
				this->components = components;
			}

			uint32_t GetStride()
			{
				uint32_t res = 0;
				for (auto& component : components)
				{
					switch (component)
					{
					case VertexComponent::UV:
						res += 2 * sizeof(float);
						break;
					case VertexComponent::DUMMY_FLOAT:
						res += sizeof(float);
						break;
					case VertexComponent::DUMMY_VEC4:
						res += 4 * sizeof(float);
						break;
					default:
						// All components except the ones listed above are made up of 3 floats
						res += 3 * sizeof(float);
					}
				}
				return res;
			}
		};

		class GLTFModel
		{
			Device* vkDevice = nullptr;
			VkQueue copyQueue;
		};
	}
}