#include "hvepch.h"
#include "helios/systems/HVEPostRenderCleanup.h"



#include "helios/world/HVEScene.h"

namespace hve {
	void HVEPostRenderCleanup::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		scene.getModelManager()->clearAllInstances();
	}
}
