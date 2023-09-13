#pragma once

#pragma once
#include "HVESystem.h"



namespace hve {
	class HVEPrepareRender : public HVESystem {
		void onUpdate(FrameInfo& frameInfo, HVEScene& scene) override;
	};
};