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
        static Ref<RHITexture> BlackCube();
        static Ref<RHITexture> WhiteArray();
        static Ref<RHITexture> BrdfLUT();

    private:
        static Ref<RHITexture> s_White;
        static Ref<RHITexture> s_Black;
        static Ref<RHITexture> s_Gray;
        static Ref<RHITexture> s_Blue;
        static Ref<RHITexture> s_BlackCube;
        static Ref<RHITexture> s_WhiteArray;
        static Ref<RHITexture> s_BrdfLUT;
    };

}
