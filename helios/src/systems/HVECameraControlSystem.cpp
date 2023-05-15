#include "systems/HVECameraControlSystem.hpp"

namespace hve
{
	void HVECameraControlSystem::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		int cameraEntityId = scene.getPlayerControlledCameraId();
		float aspect = scene.getRenderer().getAspectRatio();
		glm::vec3 rotate{ 0 };
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.lookUp) == GLFW_PRESS) rotate.x -= 1.f;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

		if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
		{
			scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation += lookSpeed * frameInfo.frameTime * glm::normalize(rotate);
		}

		scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation.x = glm::clamp(scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation.x, -1.5f, 1.5f);
		scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation.y = glm::mod(scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation.y, glm::two_pi<float>());


		float yaw = scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation.y;
		const glm::vec3 forwardDir{ sin(yaw), 0.f, cos(yaw) };
		const glm::vec3 rightDir{ forwardDir.z, 0.f, -forwardDir.x };
		const glm::vec3 upDir{ 0.f, -1.f, 0.f };

		glm::vec3 moveDir{ 0.f };

		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveUp) == GLFW_PRESS) moveDir += upDir;
		if (glfwGetKey(scene.getWindow().getGLFWwindow(), keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

		if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
		{
			scene.getEntityComponent<HVETransformComponent>(cameraEntityId).translation += moveSpeed * frameInfo.frameTime * glm::normalize(moveDir);
		}

		scene.getPlayerControlledCamera().camera.setViewYXZ(scene.getEntityComponent<HVETransformComponent>(cameraEntityId).translation, scene.getEntityComponent<HVETransformComponent>(cameraEntityId).rotation);
		scene.getPlayerControlledCamera().camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
	}
}