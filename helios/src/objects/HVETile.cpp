#include "objects/HVETile.hpp"
#include "primitives/HVEShapes.hpp"

namespace hve
{
	HVETile::HVETile(std::shared_ptr <HVEModel> quadShape, float width, float height, glm::vec3 offset) : HVEGameObject(createGameObject())
	{
		this->model = quadShape;

		this->transform.translation = offset;

		this->transform.scale = { width, height, 1.f };
	}
}