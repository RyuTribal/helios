#include "systems/HVEPrepareRender.hpp"

#include "world/HVEScene.hpp"

namespace hve
{
	void HVEPrepareRender::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		scene.getModelManager()->updateFrameIndex(frameInfo.frameIndex);
	}
}
