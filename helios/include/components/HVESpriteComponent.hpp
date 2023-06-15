#pragma once
#include "textures/HVESprite.hpp"

namespace hve
{
	struct HVESpriteComponent
	{
		HVESpriteComponent() : sprite{ nullptr } {} // Default constructor sets sprite to nullptr
		HVESpriteComponent(HVETexture* texture, glm::vec2 coordinates, glm::vec2 spriteSize) : sprite{ std::make_shared<HVESprite>(HVESprite::createSpriteFromCoordinates(texture, coordinates, spriteSize)) }
		{
		}
		std::shared_ptr<HVESprite> sprite;
	};
}
