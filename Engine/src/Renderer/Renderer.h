#pragma once
#include "Camera.h"
#include "Lights/PointLight.h"
#include "Lights/DirectionalLight.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "RenderPasses.h"
#include "DebugRenderer.h"
#include "SkyboxRenderer.h"
#include "DefaultTextures.h"
#include "PipelineCache.h"
#include "RHI/RHI.h"

namespace Engine {

    struct GLFWwindow;

    enum class AAType
    {
        None = 0,
        SSAA,
        MSAA,
    };

    enum class PPAAType
    {
        None = 0,
        FXAA
    };

    static std::string FromAATypeToString(AAType type)
    {
        switch (type)
        {
            case AAType::None: return "None";
            case AAType::SSAA: return "Super Sampling";
            case AAType::MSAA: return "Multi Sampling";
        }
        return "None";
    }

    static AAType FromStringToAAType(const std::string& type)
    {
        if (type == "Super Sampling") return AAType::SSAA;
        else if (type == "Multi Sampling") return AAType::MSAA;
        else return AAType::None;
    }

    static std::string FromPostAATypeToString(PPAAType type)
    {
        switch (type)
        {
            case PPAAType::None: return "None";
            case PPAAType::FXAA: return "FXAA";
        }
        return "None";
    }

    static PPAAType FromStringToPostAAType(const std::string& type)
    {
        if (type == "FXAA") return PPAAType::FXAA;
        else return PPAAType::None;
    }

    struct AntiAliasingSettings
    {
        AAType Type = AAType::None;
        PPAAType PostProcessing = PPAAType::None;
        int Multiplier = 2;
    };

    // Backward-compatible SkyboxSettings (uses engine texture types, not raw RHI)
    struct SkyboxSettings
    {
        Ref<TextureCube> Texture;
        float Brightness = 1.0f;
        int IrradianceResolution = 32;
        Ref<TextureCube> IrradianceTexture;
        int PrefilterResolution = 128;
        Ref<TextureCube> PrefilterMap;
        // BRDFBuffer removed (was old Framebuffer), replaced by RHI texture
        Ref<RHITexture> BRDFTexture;
    };

    struct ShadowSettings
    {
        int Resolution = 4096;
        glm::mat4 DirLightProjection;

        float LastCameraFarPlane = 500.f;
        std::vector<float> ShadowCascadeLevels{ LastCameraFarPlane / 50.0f, LastCameraFarPlane / 25.0f, LastCameraFarPlane / 10.0f, LastCameraFarPlane / 2.0f };

        glm::mat4 DirLightView;
    };

    struct RendererSettings
    {
        AntiAliasingSettings AntiAliasing{};
        SkyboxSettings Skybox{};
        ShadowSettings ShadowSettings{};
    };

    struct TextureInfo {
        uint32_t texture;
        int height;
        int width;
        int channel_number;
    };

    struct Statistics {
        double frames_per_second = 0.0;
        double frame_time_accumulator = 0.0;
        int frame_count = 0;
        double last_FPS_calculation_time = 0.0;

        int draw_calls = 0;
        int vertices_count = 0;
        int index_count = 0;

        void UpdateFPS(double currentTime, double frameTime) {
            frame_time_accumulator += frameTime;
            frame_count++;

            if (currentTime - last_FPS_calculation_time >= 1.0) {
                double avgFrameTime = frame_time_accumulator / frame_count;
                frames_per_second = 1.0 / avgFrameTime;

                frame_time_accumulator = 0.0;
                frame_count = 0;
                last_FPS_calculation_time = currentTime;
            }
        }
    };

    class Renderer
    {
    public:
        Renderer(RHIDevice* device, RHISwapchain* swapchain);
        ~Renderer();

        // Public submission API (same surface as before)
        void SubmitObject(Ref<Mesh> mesh);
        void SubmitPointLight(PointLight* point_light) { m_PointLights.push_back(point_light); }
        void SubmitDirectionalLight(DirectionalLight* light) { m_DirectionalLights.push_back(light); }

        void SubmitDebugLine(Line line);
        void SubmitDebugBox(DebugBox box);
        void SubmitDebugSphere(DebugSphere sphere);
        void SubmitDebugCapsule(DebugCapsule capsule);

        void BeginFrame(Camera* camera);
        void BeginDrawing();
        void EndFrame();

        // Default textures (backward compatible static accessors)
        static Ref<Texture2D> GetWhiteTexture();
        static Ref<Texture2D> GetBlackTexture();
        static Ref<Texture2D> GetGrayTexture();
        static Ref<Texture2D> GetBlueTexture();

        // Factory
        static void CreateRenderer(RHIDevice* device, RHISwapchain* swapchain);
        // Legacy no-arg overload: deferred init (actual init happens when Vulkan context is ready)
        static void CreateRenderer() { HVE_CORE_WARN_TAG("Renderer", "CreateRenderer() called without device/swapchain - deferred until Vulkan init"); }
        static Renderer* Get() { return s_Instance; }
        static RHIDevice* GetDevice() { return s_Instance ? s_Instance->m_Device : nullptr; }

        // Accessors
        Camera* GetCamera() { return m_CurrentCamera; }
        void SetCamera(Camera* camera) { m_CurrentCamera = camera; }

        void SetBackgroundColor(int red, int green, int blue) { m_BackgroundColor[0] = red; m_BackgroundColor[1] = green; m_BackgroundColor[2] = blue; }
        uint32_t GetSceneTextureID() { return 0; } // TODO: ImGui Vulkan descriptor set handle

        Statistics* GetStats() { return &m_Stats; }
        void ResizeViewport(int width, int height);
        void SetViewport(int width, int height);

        void SetDrawBoundingBoxes(bool should_draw) { m_DrawBoundingBox = should_draw; }

        RendererSettings& GetSettings() { return m_Settings; }
        void SetAntiAliasing(AntiAliasingSettings& settings);
        void SetSkybox(SkyboxSettings& settings);

        PipelineCache* GetPipelineCache() { return &m_PipelineCache; }

    private:
        void ResetStats();

        // Cascade shadow map helpers
        std::vector<glm::mat4> ComputeCascadeLightMatrices(const glm::vec3& lightDir);
        static glm::mat4 ComputeLightSpaceMatrix(Camera* camera, float nearPlane, float farPlane, const glm::vec3& lightDir);

    private:
        static Renderer* s_Instance;

        RHIDevice* m_Device;
        RHISwapchain* m_Swapchain;

        // Render passes
        DepthPrePass m_DepthPrePass;
        ShadowPass m_ShadowPass;
        LightCullingPass m_LightCulling;
        ForwardPass m_ForwardPass;
        TonemapPass m_TonemapPass;
        SkyboxRenderer m_SkyboxRenderer;
        DebugRenderer m_DebugRenderer;
        PipelineCache m_PipelineCache;

        // Per-frame command buffer
        Ref<RHICommandBuffer> m_CommandBuffer;

        // Submission queues (cleared each frame)
        std::vector<Ref<Mesh>> m_Meshes;
        std::vector<PointLight*> m_PointLights;
        std::vector<DirectionalLight*> m_DirectionalLights;

        std::vector<Line> m_DebugLines;
        std::vector<DebugBox> m_DebugBoxes;
        std::vector<DebugSphere> m_DebugSpheres;
        std::vector<DebugCapsule> m_DebugCapsules;

        Camera* m_CurrentCamera = nullptr;
        RendererSettings m_Settings;
        Statistics m_Stats;
        int m_BackgroundColor[3] = { 0, 0, 0 };

        float m_Width = 1280, m_Height = 720;

        bool m_DrawBoundingBox = false;

        const float m_Exposure = 1.0f;

        // Default texture wrappers for backward compatibility
        static Ref<Texture2D> s_WhiteTexWrap;
        static Ref<Texture2D> s_BlackTexWrap;
        static Ref<Texture2D> s_GrayTexWrap;
        static Ref<Texture2D> s_BlueTexWrap;
    };

    // This is so the spd log library can print this data structure
    inline std::ostream& operator<<(std::ostream& os, const Vertex& v) {
        return os << "{" << "x: " << v.coordinates.x << ", y: " << v.coordinates.y << ", z: " << v.coordinates.z << "}";
    }

    inline std::ostream& operator<<(std::ostream& os, const std::vector<Vertex>& vec) {
        os << "[ \n";
        int row_items_count = 0;
        const int max_row_items = 2;
        for (size_t i = 0; i < vec.size(); ++i) {
            if(row_items_count == 0)
            {
                os << "\t";
            }
            os << vec[i];
            if (i < vec.size() - 1) {
                os << ", ";
                row_items_count++;
                if(row_items_count >= max_row_items)
                {
                    os << "\n";
                    row_items_count = 0;
                }
            }
        }

        os << "\n";

        return os << "]";
    }

}
