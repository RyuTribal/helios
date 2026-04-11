#include "console.h"
#include "../editor_state.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/ecs/world.h>
#include <helios/core/log_system.h>

#include <imgui.h>

#include <string>
#include <vector>

namespace helios::editor {

// Categories group multiple log channels under one tab.
struct ConsoleCategory {
    const char* label;
    const char* channels[12];  // null-terminated list of channel name substrings
};

static const ConsoleCategory s_categories[] = {
    {
        ICON_MDI_GAMEPAD_VARIANT " Game",
        {"Script", "Game", "Scene", nullptr}
    },
    {
        ICON_MDI_COG " Engine",
        {"Core", "App", "ECS", "Scheduler", "Assets", "Input", "Window",
         "Physics", "Audio", "Editor", "EditorMain", nullptr}
    },
    {
        ICON_MDI_MONITOR " Render",
        {"Renderer", "Render", "ForwardPlus", "Graph", nullptr}
    },
};
static constexpr int NUM_CATEGORIES = static_cast<int>(std::size(s_categories));

static bool message_matches_category(const std::string& msg, const ConsoleCategory& cat) {
    for (int i = 0; cat.channels[i] != nullptr; i++) {
        std::string pattern = std::string("[") + cat.channels[i] + "]";
        if (msg.find(pattern) != std::string::npos) return true;
    }
    return false;
}

static ImVec4 get_level_color(const std::string& msg) {
    if (msg.find("[error]") != std::string::npos || msg.find("[critical]") != std::string::npos)
        return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    if (msg.find("[warn") != std::string::npos)
        return ImVec4(1.0f, 0.9f, 0.3f, 1.0f);
    if (msg.find("[debug]") != std::string::npos)
        return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    if (msg.find("[trace]") != std::string::npos)
        return ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

void console_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_console) return;

    ImGui::Begin("Console");

    auto* log_system = LogSystem::instance();
    if (!log_system) {
        ImGui::Text("LogSystem not available");
        ImGui::End();
        return;
    }

    static int selected_tab = 0;

    if (ImGui::BeginTabBar("ConsoleCategories")) {
        for (int i = 0; i < NUM_CATEGORIES; i++) {
            if (ImGui::BeginTabItem(s_categories[i].label)) {
                selected_tab = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    auto messages = log_system->get_recent_messages(500);

    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& msg : messages) {
        if (!message_matches_category(msg, s_categories[selected_tab])) continue;
        ImVec4 color = get_level_color(msg);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(msg.c_str());
        ImGui::PopStyleColor();
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace helios::editor
