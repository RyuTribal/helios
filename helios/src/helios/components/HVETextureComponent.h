#pragma once
#include <string>

#include "helios/textures/HVETexture.h"



namespace hve
{
	struct HVETextureComponent
	{
		HVETextureComponent() : texture{ nullptr } {}
		HVETextureComponent(HVETexture* tex) : texture{ tex } {}
		HVETexture* texture;
	};
}
