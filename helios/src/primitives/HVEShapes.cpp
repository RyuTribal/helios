#include "primitives/HVEShapes.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace hve
{
	std::unique_ptr<HVEModel> HVEShapes::drawQuad(HVEDevice& device, glm::vec3 offset)
	{
		HVEModel::Builder modelBuilder{};

		modelBuilder.vertices = {
			{{-0.5f, -0.5f, .0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Bottom-left corner (Red)
			{{0.5f, -0.5f, .0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // Bottom-right corner (Green)
			{{0.5f, 0.5f, .0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // Top-right corner (Blue)
			{{-0.5f, 0.5f, .0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}   // Top-left corner (Yellow)
		};

		modelBuilder.indices = {
			0, 1, 2, // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0  // Second triangle (top-right, top-left, bottom-left)
		};
		for (auto& v : modelBuilder.vertices) {
			v.position += offset;
		}
		return std::make_unique<HVEModel>(device, modelBuilder);
	}
}
