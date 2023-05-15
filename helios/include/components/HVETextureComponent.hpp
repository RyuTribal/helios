#pragma once
#include <string>

#include "textures/HVETexture.hpp"

namespace hve
{
	struct HVETextureComponent
	{
		HVETextureComponent() : texture{ nullptr } {}
		HVETextureComponent(HVETexture* tex) : texture{ tex } {}
		HVETexture* texture;
	};
}
