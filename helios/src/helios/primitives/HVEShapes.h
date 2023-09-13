#pragma once

#include <memory>

#include "helios/graphics/HVEModel.h"




namespace hve
{
	class HVEShapes
	{
	public:
		static std::unique_ptr<HVEModel> drawQuad(HVEDevice& device);
	};
}
