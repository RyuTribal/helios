#pragma once
#include "HVESystem.hpp"

namespace hve {
	class HVEPostRenderCleanup : public HVESystem {
		void onUpdate(FrameInfo& frameInfo, HVEScene& scene) override;
	};
};
