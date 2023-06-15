#include "systems/HVEPostRenderCleanup.hpp"

#include "world/HVEScene.hpp"

namespace hve {
	void HVEPostRenderCleanup::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		scene.getModelManager()->clearAllInstances();
	}
}
