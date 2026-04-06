#pragma once
#include "RHI/RHI.h"

namespace Engine {

    class DefaultTextures {
    public:
        static void Init(RHIDevice* device);
        static void Shutdown();

        static Ref<RHITexture> White();
        static Ref<RHITexture> Black();
        static Ref<RHITexture> Gray();
        static Ref<RHITexture> Blue();

    private:
        static Ref<RHITexture> s_White;
        static Ref<RHITexture> s_Black;
        static Ref<RHITexture> s_Gray;
        static Ref<RHITexture> s_Blue;
    };

}
