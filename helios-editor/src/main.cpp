#include <helios/ecs/app.h>
#include <helios/core/logging.h>
#include <helios/core/log_plugin.h>
#include <helios/window/window_plugin.h>
#include <helios/input/input_plugin.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
#include <helios/assets/asset_plugin.h>

#include "editor_plugin.h"

// Physics & Audio
#include "interface/physics_factory.h"
#include "interface/audio_factory.h"

// Scripting
#include <helios/script/scripting_plugin.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace helios;

HELIOS_DEFINE_LOG_CHANNEL(EditorMain);

int main(int argc, char** argv) {
    // Parse project path from command line
    std::filesystem::path project_path;
    for (int i = 1; i < argc; ++i) {
        std::filesystem::path arg{argv[i]};
        if (arg.extension() == ".hveproject") {
            project_path = arg;
            break;
        }
    }

    std::string window_title = "Helios Editor";
    if (!project_path.empty()) {
        window_title = project_path.stem().string() + " - Helios Editor";
    }

    App app;
    app.enable_parallel();

    // Logging first — available for all subsequent plugin builds
    app.add_plugin(LogPlugin{LogConfig{
        .enable_file_sink = false,
        .default_level = LogLevel::Debug,
    }});

    HELIOS_LOG(EditorMain, Info, "=== Helios Editor ===");

    // Engine plugins
    app.add_plugin(WindowPlugin{
        .primary_window = {
            .title = window_title,
            .width = 1920,
            .height = 1080,
        },
    });
    app.add_plugin(InputPlugin{});

    // Pre-parse project present mode so swapchain is created correctly at startup
    rhi::PresentMode initial_pm = rhi::PresentMode::Fifo;
    if (!project_path.empty() && std::filesystem::exists(project_path)) {
        // Quick read of the project file for PresentMode
        std::ifstream pf(project_path);
        std::string line;
        while (std::getline(pf, line)) {
            if (line.find("PresentMode:") != std::string::npos) {
                if (line.find("Immediate") != std::string::npos) initial_pm = rhi::PresentMode::Immediate;
                else if (line.find("Mailbox") != std::string::npos) initial_pm = rhi::PresentMode::Mailbox;
                break;
            }
        }
    }

    app.add_plugin(RenderPlugin{.mode = RenderMode::Offscreen, .initial_present_mode = initial_pm});
    app.add_plugin(AssetPlugin{AssetPluginConfig{.asset_root = "."}});
    app.add_plugin(ForwardPlusPlugin{});
    app.add_plugin(physics::DefaultPhysicsPlugin{});
    app.add_plugin(audio::DefaultAudioPlugin{});

    // Editor plugin (adds ImGui, panels, undo/redo, gizmos)
    app.add_plugin(editor::EditorPlugin{
        .config = { .project_path = project_path },
    });

    app.run();
    return 0;
}
