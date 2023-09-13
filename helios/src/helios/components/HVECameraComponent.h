#pragma once
#include "helios/graphics/HVECamera.h"

namespace hve
{
	struct HVECameraComponent
	{
		HVECamera camera{};
		float zoom;

		HVECameraComponent(float zoom) : zoom{ zoom } {}
		HVECameraComponent() : zoom{1.f} {}
	};
}
