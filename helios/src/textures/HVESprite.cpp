#include "textures/HVESprite.hpp"

namespace hve
{
	HVESprite::HVESprite(HVETexture* texture, const glm::vec2& top_left_corner, const glm::vec2& bottom_right_corner) : spriteSheet{texture}
	{
		spriteSheetCoordinates[0] = { top_left_corner.x, top_left_corner.y };
		spriteSheetCoordinates[1] = { bottom_right_corner.x, top_left_corner.y };
		spriteSheetCoordinates[2] = { bottom_right_corner.x, bottom_right_corner.y };
		spriteSheetCoordinates[3] = { top_left_corner.x, bottom_right_corner.y };
	}
	HVESprite HVESprite::createSpriteFromCoordinates(HVETexture* texture, glm::vec2& coordinates, glm::vec2& spriteSize)
	{
		glm::vec2 top_left_corner = { (coordinates.x * spriteSize.x) / texture->getWidth(), (coordinates.y * spriteSize.y) / texture->getHeight() };
		glm::vec2 bottom_right_corner = { ((coordinates.x + 1) * spriteSize.x - 1) / texture->getWidth(), ((coordinates.y + 1) * spriteSize.y - 1) / texture->getHeight() };


		return HVESprite(texture, top_left_corner, bottom_right_corner);
	}
}
