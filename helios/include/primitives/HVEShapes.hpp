#pragma once

#include <memory>

#include "graphics/HVEModel.hpp"

namespace hve
{
	class HVEShapes
	{
	public:
		static std::unique_ptr<HVEModel> drawQuad(HVEDevice& device, glm::vec3 offset);
	};
}
