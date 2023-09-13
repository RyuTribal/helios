#pragma once
#include <glm/vec2.hpp>


namespace hve
{
	struct HVEColliderComponent
	{
		enum class Shape {Rectangle, Circle};

		Shape shape;
		glm::vec2 size;

		HVEColliderComponent(Shape sh = Shape::Rectangle, const glm::vec2& sz = glm::vec2(1.0f))
			: shape(sh), size(sz) {}
	};
}
