#include "hvepch.h"
#include "helios/textures/HVESprite.h"



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
		int tex_width = texture ? texture->getWidth() : 1;
		int tex_height = texture ? texture->getHeight() : 1;
		glm::vec2 top_left_corner = { (coordinates.x * spriteSize.x) / tex_width, (coordinates.y * spriteSize.y) / tex_height };
		glm::vec2 bottom_right_corner = { ((coordinates.x + 1) * spriteSize.x - 1) / tex_width, ((coordinates.y + 1) * spriteSize.y - 1) / tex_height };


		return HVESprite(texture, top_left_corner, bottom_right_corner);
	}
}
