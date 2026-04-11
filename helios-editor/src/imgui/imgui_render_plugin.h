#pragma once

#include <helios/ecs/app.h>
#include <imgui.h>

namespace helios::rhi { class Texture; }

namespace helios::editor {

struct ImGuiState {
    bool initialized = false;
};

struct ImGuiRenderPlugin {
    void build(App& app);
};

/// Register an RHI texture with ImGui for display via ImGui::Image.
/// Returns an opaque ImTextureID. The texture must be in ShaderReadOnly layout.
ImTextureID imgui_register_texture(rhi::Texture& texture);

/// Unregister a previously registered texture. Pass the ID returned by register.
void imgui_unregister_texture(ImTextureID id);

} // namespace helios::editor
