#pragma once
#include <glm/vec2.hpp>

#include "HVETexture.h"



namespace hve
{
	class HVESprite
	{
	public:
		HVESprite(HVETexture* texture, const glm::vec2& top_left_corner, const glm::vec2& bottom_right_corner);

		const glm::vec2* getSpriteSheetCoordinates() const { return spriteSheetCoordinates; }
		const HVETexture* getSpriteSheet() const { return spriteSheet; }

		static HVESprite createSpriteFromCoordinates(HVETexture* texture, glm::vec2& coordinates , glm::vec2& spriteSize);
	private:
		HVETexture* spriteSheet;
		glm::vec2 spriteSheetCoordinates[4];
	};
}
