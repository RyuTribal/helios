#pragma once
#include "graphics/HVEGameObject.hpp"

namespace hve
{
	class HVETile : public HVEGameObject
	{
	public:
		HVETile(std::shared_ptr <HVEModel> quadShape, float width, float height, glm::vec3 offset);

	};
}
