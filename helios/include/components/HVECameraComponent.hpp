#pragma once
#include "graphics/HVECamera.hpp"

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
