#include <gtest/gtest.h>
#include "helios/rhi/rhi_types.h"

using namespace helios::rhi;

TEST(RHITypes, BufferUsageBitfieldOr) {
    auto usage = BufferUsage::Vertex | BufferUsage::Index;
    EXPECT_TRUE(has_flag(usage, BufferUsage::Vertex));
    EXPECT_TRUE(has_flag(usage, BufferUsage::Index));
    EXPECT_FALSE(has_flag(usage, BufferUsage::Uniform));
}

TEST(RHITypes, TextureUsageBitfieldAnd) {
    auto usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
    auto masked = usage & TextureUsage::Sampled;
    EXPECT_TRUE(has_flag(masked, TextureUsage::Sampled));
}

TEST(RHITypes, ShaderStageBitfieldCombine) {
    auto stage = ShaderStage::Vertex | ShaderStage::Fragment;
    EXPECT_TRUE(has_flag(stage, ShaderStage::Vertex));
    EXPECT_TRUE(has_flag(stage, ShaderStage::Fragment));
    EXPECT_FALSE(has_flag(stage, ShaderStage::Compute));
}

TEST(RHITypes, ShaderStageBitfieldNot) {
    auto all = ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
    auto without_compute = all & ~ShaderStage::Compute;
    EXPECT_TRUE(has_flag(without_compute, ShaderStage::Vertex));
    EXPECT_FALSE(has_flag(without_compute, ShaderStage::Compute));
}

TEST(RHITypes, TextureDescDefaults) {
    TextureDesc desc;
    EXPECT_EQ(desc.width, 1u);
    EXPECT_EQ(desc.height, 1u);
    EXPECT_EQ(desc.format, TextureFormat::RGBA8);
    EXPECT_EQ(desc.mip_levels, 1u);
    EXPECT_EQ(desc.sampler, SamplerMode::Repeat);
}

TEST(RHITypes, BufferDescDefaults) {
    BufferDesc desc;
    EXPECT_EQ(desc.size, 0u);
    EXPECT_EQ(desc.usage, BufferUsage::Vertex);
    EXPECT_EQ(desc.access, MemoryAccess::GPU_Only);
}

TEST(RHITypes, BufferUsageOrAssign) {
    auto usage = BufferUsage::Vertex;
    usage |= BufferUsage::Index;
    EXPECT_TRUE(has_flag(usage, BufferUsage::Vertex));
    EXPECT_TRUE(has_flag(usage, BufferUsage::Index));
}

TEST(RHITypes, TextureUsageAndAssign) {
    auto usage = TextureUsage::Sampled | TextureUsage::Storage;
    usage &= TextureUsage::Sampled;
    EXPECT_TRUE(has_flag(usage, TextureUsage::Sampled));
    EXPECT_FALSE(has_flag(usage, TextureUsage::Storage));
}

TEST(RHITypes, ClearValuesDefaults) {
    ClearValues clear;
    EXPECT_FLOAT_EQ(clear.color[0], 0.0f);
    EXPECT_FLOAT_EQ(clear.color[3], 1.0f);
    EXPECT_FLOAT_EQ(clear.depth, 1.0f);
    EXPECT_EQ(clear.stencil, 0u);
}

TEST(RHITypes, RenderPassDescDefaults) {
    RenderPassDesc desc;
    EXPECT_TRUE(desc.color_attachments.empty());
    EXPECT_FALSE(desc.has_depth);
}
