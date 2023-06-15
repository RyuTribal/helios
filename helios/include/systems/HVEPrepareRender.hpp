#pragma once

#pragma once
#include "HVESystem.hpp"

namespace hve {
	class HVEPrepareRender : public HVESystem {
		void onUpdate(FrameInfo& frameInfo, HVEScene& scene) override;
	};
};