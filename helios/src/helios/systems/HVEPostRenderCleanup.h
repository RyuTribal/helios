#pragma once
#include "HVESystem.h"



namespace hve {
	class HVEPostRenderCleanup : public HVESystem {
		void onUpdate(FrameInfo& frameInfo, HVEScene& scene) override;
	};
};
