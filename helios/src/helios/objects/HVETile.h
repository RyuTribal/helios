#pragma once
#include "helios/graphics/HVEGameObject.h"



namespace hve
{
	class HVETile : public HVEGameObject
	{
	public:
		HVETile(std::shared_ptr <HVEModel> quadShape, float width, float height, glm::vec3 offset);

	};
}
