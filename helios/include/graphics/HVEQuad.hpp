#pragma once


#include "HVEModel.hpp"

#include <memory>

namespace hve {

	struct Transform2dComponent
	{
		glm::vec2 translation{};
		glm::vec2 scale{ 1.f, 1.f };
		float rotation;

		glm::mat2 mat2()
		{
			const float s = glm::sin(rotation);
			const float c = glm::cos(rotation);
			glm::mat2 rotMatrix{ {c, s}, {-s, c} };

			glm::mat2 scaleMat{ {scale.x, .0f} , { .0f, scale.y } };
			return rotMatrix * scaleMat;
		}
	};

	class HVEQuad
	{
	};
}