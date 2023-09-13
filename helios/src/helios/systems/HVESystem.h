#pragma once
#include "helios/graphics/HVEFrameInfo.h"




namespace hve
{
    class HVEScene;  // Forward declaration

    class HVESystem
    {
    public:
        virtual ~HVESystem() {}

        virtual void onAttach() {}

        // Called when the system is removed from the engine or game world
        virtual void onDetach() {}

        // Called every frame or tick to update the system
        virtual void onUpdate(FrameInfo& frameInfo, HVEScene& scene) {}
    };
}
