#pragma once

// rhi.h -- Includes all abstract RHI interface headers.
//
// Callers include this single header to get access to the abstract types:
//   helios::rhi::Device, Texture, Buffer, Shader, Pipeline, etc.
//
// Backend selection is done at runtime via rhi::create_device().
// See rhi_factory.h for the factory function.

#include "helios/rhi/rhi_types.h"
#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_texture.h"
#include "helios/rhi/rhi_buffer.h"
#include "helios/rhi/rhi_shader.h"
#include "helios/rhi/rhi_pipeline.h"
#include "helios/rhi/rhi_command_buffer.h"
#include "helios/rhi/rhi_swapchain.h"
#include "helios/rhi/rhi_descriptor.h"
#include "helios/rhi/rhi_render_pass.h"
#include "helios/rhi/rhi_framebuffer.h"
#include "helios/rhi/rhi_factory.h"
