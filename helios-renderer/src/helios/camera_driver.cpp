#include "helios/camera_driver.h"
#include "helios/render_settings.h"
#include "helios/rhi/rhi_command_buffer.h"
#include "helios/rhi/rhi_texture.h"
#include "helios/core/log_macros.h"

#include <unordered_set>

HELIOS_DECLARE_LOG_CHANNEL(Render);

namespace helios {

void camera_driver(
    World& world,
    ResMut<RenderContext> ctx,
    Res<renderer::FramePacket> packet,
    ResMut<CameraRenderSchedules> schedules,
    Res<RenderScheduleRegistry> registry,
    Res<RenderSettings> settings)
{
    if (!ctx->frame_active) return;

    RenderScheduleLabel default_label = registry->find("forward_plus");

    // Track which target is currently bound so we can batch cameras
    // that share the same render target into one pass.
    rhi::Texture* current_color = nullptr;
    rhi::Texture* current_depth = nullptr;
    bool pass_open = false;

    auto begin_pass = [&](rhi::Texture* color, rhi::Texture* depth,
                          uint32_t w, uint32_t h, bool clear) {
        if (pass_open) {
            ctx->cmd->end_rendering();
            // Transition previous target to shader-readable
            if (current_color) {
                ctx->cmd->transition_image(*current_color,
                    rhi::TextureLayout::ColorAttachment,
                    rhi::TextureLayout::ShaderReadOnly);
            }
        }
        rhi::ClearValues cv;
        if (clear) {
            cv.color[0] = settings->clear_color.r;
            cv.color[1] = settings->clear_color.g;
            cv.color[2] = settings->clear_color.b;
            cv.color[3] = settings->clear_color.a;
        }
        cv.depth = 1.0f;
        ctx->cmd->begin_rendering(color, depth, cv, w, h);
        current_color = color;
        current_depth = depth;
        pass_open = true;
    };

    // Cameras are already sorted by order in extract_render_data.
    for (uint32_t i = 0; i < packet->camera_views.size(); ++i) {
        const auto& view = packet->camera_views[i];

        // Resolve render target for this camera
        rhi::Texture* tgt_color = view.target_color
            ? view.target_color : ctx->scene_color;
        rhi::Texture* tgt_depth = view.target_depth
            ? view.target_depth : ctx->scene_depth;
        uint32_t tgt_w = view.target_color ? view.target_width : ctx->scene_width;
        uint32_t tgt_h = view.target_color ? view.target_height : ctx->scene_height;

        // Switch pass if target changed
        if (tgt_color != current_color || tgt_depth != current_depth) {
            begin_pass(tgt_color, tgt_depth, tgt_w, tgt_h,
                       view.clear_mode == renderer::CameraClearMode::SolidColor);
        }

        RenderScheduleLabel label{view.schedule_label_value};
        if (label.value == 0) label = default_label;

        if (!schedules->has_steps(label)) {
            HELIOS_LOG(Render, Warn,
                "Camera {} has unknown render schedule '{}', falling back to default",
                i, registry->name_of(label));
            label = default_label;
        }

        schedules->run(label, world, view, i);
    }

    // Close the last open pass and transition
    if (pass_open) {
        ctx->cmd->end_rendering();
        if (current_color) {
            ctx->cmd->transition_image(*current_color,
                rhi::TextureLayout::ColorAttachment,
                rhi::TextureLayout::ShaderReadOnly);
        }
    }

    // Prune camera targets not referenced by any camera view this frame.
    // Collect IDs of targets still in use, then remove the rest.
    {
        std::unordered_set<uint32_t> active_ids;
        for (const auto& view : packet->camera_views) {
            // A camera view references a target if its target_color matches
            // one of our managed targets.
            if (view.target_color) {
                for (const auto& [id, tgt] : ctx->camera_targets) {
                    if (tgt.color.get() == view.target_color) {
                        active_ids.insert(id);
                        break;
                    }
                }
            }
        }
        for (auto it = ctx->camera_targets.begin(); it != ctx->camera_targets.end(); ) {
            if (active_ids.count(it->first) == 0) {
                if (it->second.color) ctx->device->defer_destroy(std::move(it->second.color));
                if (it->second.depth) ctx->device->defer_destroy(std::move(it->second.depth));
                it = ctx->camera_targets.erase(it);
            } else {
                ++it;
            }
        }
    }

    // If no cameras rendered at all, still need scene_color in ShaderReadOnly
    // for ImGui to sample (even if it's blank).
    if (!pass_open && ctx->scene_color) {
        rhi::ClearValues cv;
        cv.color[0] = cv.color[1] = cv.color[2] = 0.0f; cv.color[3] = 1.0f;
        cv.depth = 1.0f;
        ctx->cmd->begin_rendering(ctx->scene_color, ctx->scene_depth,
                                   cv, ctx->scene_width, ctx->scene_height);
        ctx->cmd->end_rendering();
        ctx->cmd->transition_image(*ctx->scene_color,
            rhi::TextureLayout::ColorAttachment,
            rhi::TextureLayout::ShaderReadOnly);
    }
}

} // namespace helios
