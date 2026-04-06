# RHI Abstraction + Vulkan Backend — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all OpenGL rendering with a Vulkan backend behind a Rendering Hardware Interface (RHI) abstraction layer.

**Architecture:** Engine code talks to RHI interfaces (pure virtual classes). Vulkan backend implements all RHI interfaces using VMA for memory and vk-bootstrap for instance/device setup. GLSL shaders are pre-compiled to SPIR-V via glslc. Forward+ rendering pipeline is preserved.

**Tech Stack:** Vulkan 1.3+, VMA (Vulkan Memory Allocator), vk-bootstrap, glslc (SPIR-V compiler), GLFW (Vulkan surface), C++20

**Spec:** `docs/superpowers/specs/2026-04-06-rhi-vulkan-design.md`

**IMPORTANT — Modern Vulkan Practices:**
- **Target Vulkan 1.3 minimum.** Use features promoted from extensions: dynamic rendering (`VK_KHR_dynamic_rendering`), synchronization2 (`VK_KHR_synchronization2`), extended dynamic state. Do NOT use Vulkan 1.0 patterns from old tutorials.
- **Dynamic Rendering** — prefer `vkCmdBeginRendering` / `vkCmdEndRendering` over `VkRenderPass` + `VkFramebuffer` where possible. This eliminates render pass and framebuffer object boilerplate. The RHI can still abstract this via `BeginRenderPass()` but the Vulkan backend uses the 1.3 dynamic rendering path internally.
- **Synchronization2** — use `VkPipelineStageFlags2` and `vkCmdPipelineBarrier2` instead of legacy pipeline barriers.
- **Debug extensively** — In Debug builds, enable ALL validation layers via `VK_LAYER_KHRONOS_validation`. Set up `VkDebugUtilsMessengerEXT` that pipes all Vulkan messages (info, warning, error, performance) through `HVE_CORE_*_TAG("Vulkan", ...)`. Use `vkSetDebugUtilsObjectNameEXT` to name every Vulkan object (buffers, images, pipelines, etc.) so validation errors show human-readable names. Add verbose object creation/destruction logging in Debug mode.

**Prerequisites:**
- Vulkan SDK installed (check: `pkg-config --modversion vulkan`, must be 1.3+)
- `glslc` available (check: `which glslc`)
- GPU driver with Vulkan 1.3 support
- GLFW with Vulkan support (already vendored, has `_GLFW_X11` + Vulkan loader)

---

## Phase 1: RHI Interfaces + Vulkan Init (Triangle on Screen)

### Task 1: Vendor VMA and vk-bootstrap

**Files:**
- Create: `Engine/vendor/vma/vk_mem_alloc.h`
- Create: `Engine/vendor/vk-bootstrap/VkBootstrap.h`
- Create: `Engine/vendor/vk-bootstrap/VkBootstrap.cpp`
- Create: `Engine/vendor/vk-bootstrap/VkBootstrapDispatch.h`
- Modify: `Dependencies.lua`
- Modify: `Engine/premake5.lua`
- Modify: `premake5.lua`

- [ ] **Step 1: Download VMA**

```bash
curl -sSL https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h \
  -o Engine/vendor/vma/vk_mem_alloc.h
```

- [ ] **Step 2: Download vk-bootstrap**

```bash
mkdir -p Engine/vendor/vk-bootstrap
curl -sSL https://raw.githubusercontent.com/charles-lunarg/vk-bootstrap/main/src/VkBootstrap.h \
  -o Engine/vendor/vk-bootstrap/VkBootstrap.h
curl -sSL https://raw.githubusercontent.com/charles-lunarg/vk-bootstrap/main/src/VkBootstrap.cpp \
  -o Engine/vendor/vk-bootstrap/VkBootstrap.cpp
curl -sSL https://raw.githubusercontent.com/charles-lunarg/vk-bootstrap/main/src/VkBootstrapDispatch.h \
  -o Engine/vendor/vk-bootstrap/VkBootstrapDispatch.h
```

- [ ] **Step 3: Update Dependencies.lua**

Add:
```lua
IncludeDir["VulkanSDK"] = "/usr/include"
IncludeDir["vma"] = "%{wks.location}/Engine/vendor/vma"
IncludeDir["vkbootstrap"] = "%{wks.location}/Engine/vendor/vk-bootstrap"
```

- [ ] **Step 4: Update Engine/premake5.lua**

Add to `includedirs`:
```lua
"%{IncludeDir.VulkanSDK}",
"%{IncludeDir.vma}",
"%{IncludeDir.vkbootstrap}",
```

Add to shared `files`:
```lua
"vendor/vk-bootstrap/VkBootstrap.cpp",
```

In `filter "system:linux"` links, add `"vulkan"` and keep existing libs.

Remove `Glad` from links (will be done in Phase 3, but prepare by noting it).

- [ ] **Step 5: Verify compilation**

```bash
vendor/premake/premake5 gmake2 && make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -5
```

Expected: Engine compiles (vk-bootstrap and VMA are included but not yet used).

- [ ] **Step 6: Commit**

```bash
git add Engine/vendor/vma/ Engine/vendor/vk-bootstrap/ Dependencies.lua Engine/premake5.lua
git commit -m "build: vendor VMA and vk-bootstrap for Vulkan backend"
```

---

### Task 2: Create RHI interface headers

**Files:**
- Create: `Engine/src/RHI/RHITypes.h`
- Create: `Engine/src/RHI/RHIResources.h`
- Create: `Engine/src/RHI/RHIDescriptor.h`
- Create: `Engine/src/RHI/RHICommandBuffer.h`
- Create: `Engine/src/RHI/RHIDevice.h`
- Create: `Engine/src/RHI/RHISwapchain.h`
- Create: `Engine/src/RHI/RHI.h` (umbrella include)

These are pure interface headers with no implementation. See the spec section "RHI Interfaces" for the exact class definitions.

- [ ] **Step 1: Create RHITypes.h**

All enums and descriptor structs from the spec: `ImageFormat`, `TextureType`, `TextureUsage`, `BufferUsage`, `MemoryAccess`, `ShaderStage`, `IndexType`, `CullMode`, `DepthCompare`, `BlendMode`, `LoadOp`, `StoreOp`, `DescriptorType`, and all `*Desc` structs (`TextureDesc`, `BufferDesc`, `ShaderDesc`, `VertexAttribute`, `VertexLayout`, `RenderState`, `AttachmentDesc`, `RenderPassDesc`, `FramebufferDesc`, `GraphicsPipelineDesc`, `ComputePipelineDesc`, `DescriptorBinding`, `DescriptorSetLayoutDesc`, `DescriptorWrite`, `ClearValues`, `BarrierDesc`).

Use `uint32_t` flags for bitfield enums (TextureUsage, BufferUsage, ShaderStage). Include `<cstdint>`, `<vector>`, `<string>`, `<glm/glm.hpp>`.

- [ ] **Step 2: Create RHIResources.h**

Abstract base classes: `RHITexture`, `RHIBuffer`, `RHIShader`, `RHIPipeline`, `RHIRenderPass`, `RHIFramebuffer`. Each has virtual destructor. `RHIBuffer` has `SetData`, `Map`, `Unmap`. `RHIFramebuffer` has `GetWidth`, `GetHeight`. See spec for exact signatures.

- [ ] **Step 3: Create RHIDescriptor.h**

Abstract classes: `RHIDescriptorSetLayout`, `RHIDescriptorSet`. Both just have virtual destructors.

- [ ] **Step 4: Create RHICommandBuffer.h**

Pure virtual class with all command recording methods from the spec: `Begin`, `End`, `BeginRenderPass`, `EndRenderPass`, `BindPipeline`, `SetViewport`, `SetScissor`, `BindVertexBuffer`, `BindIndexBuffer`, `BindDescriptorSet`, `PushConstants`, `Draw`, `DrawIndexed`, `Dispatch`, `PipelineBarrier`, `CopyBuffer`.

- [ ] **Step 5: Create RHIDevice.h**

Pure virtual factory class: `CreateTexture`, `CreateBuffer`, `CreateShader`, `CreateGraphicsPipeline`, `CreateComputePipeline`, `CreateRenderPass`, `CreateFramebuffer`, `CreateDescriptorSetLayout`, `AllocateDescriptorSet`, `UpdateDescriptorSet`, `CreateCommandBuffer`, `SubmitCommandBuffer`, `WaitIdle`.

- [ ] **Step 6: Create RHISwapchain.h**

Pure virtual: `AcquireNextImage`, `Present`, `Resize`, `GetCurrentImage`, `GetRenderPass`, `GetCurrentFramebuffer`, `GetWidth`, `GetHeight`.

- [ ] **Step 7: Create RHI.h umbrella header**

```cpp
#pragma once
#include "RHITypes.h"
#include "RHIResources.h"
#include "RHIDescriptor.h"
#include "RHICommandBuffer.h"
#include "RHIDevice.h"
#include "RHISwapchain.h"
```

- [ ] **Step 8: Commit**

```bash
git add Engine/src/RHI/
git commit -m "feat: add RHI interface headers (Device, CommandBuffer, Swapchain, Resources)"
```

---

### Task 3: Implement VulkanContext and VulkanDevice

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanContext.h`
- Create: `Engine/src/Renderer/Vulkan/VulkanContext.cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanDevice.h`
- Create: `Engine/src/Renderer/Vulkan/VulkanDevice.cpp`

- [ ] **Step 1: Create VulkanContext**

VulkanContext manages VkInstance and debug messenger. Uses vk-bootstrap for creation.

```cpp
// VulkanContext.h
#pragma once
#include <vulkan/vulkan.h>

namespace Engine {
    class VulkanContext {
    public:
        static bool Init(const char* appName, bool enableValidation);
        static void Shutdown();
        static VkInstance GetInstance();
        static VkSurfaceKHR CreateSurface(void* nativeWindow); // GLFWwindow*
    private:
        static VkInstance s_Instance;
        static VkDebugUtilsMessengerEXT s_DebugMessenger;
    };
}
```

Implementation uses `vkb::InstanceBuilder` from vk-bootstrap:
- Enable validation layers in Debug builds (`VK_LAYER_KHRONOS_validation`)
- Set API version to Vulkan 1.3
- Request `VK_EXT_debug_utils` extension
- Set up `VkDebugUtilsMessengerEXT` with a callback that:
  - Maps `VK_DEBUG_UTILS_MESSAGE_SEVERITY_*` to `HVE_CORE_TRACE/WARN/ERROR_TAG("Vulkan", ...)`
  - Logs the message ID name and message text
  - Returns `VK_FALSE` (don't abort)
- Log ALL severity levels in Debug (verbose, info, warning, error, performance)
- `CreateSurface` calls `glfwCreateWindowSurface()`

Add a static helper `SetDebugName(VkDevice, VkObjectType, uint64_t handle, const char* name)` that calls `vkSetDebugUtilsObjectNameEXT`. Use this throughout all Vulkan backend classes to name every object on creation (e.g., `SetDebugName(device, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, "LightSSBO")`).

- [ ] **Step 2: Create VulkanDevice**

VulkanDevice implements `RHIDevice`. Holds VkPhysicalDevice, VkDevice, VkQueue, VmaAllocator.

Uses `vkb::PhysicalDeviceSelector` and `vkb::DeviceBuilder` from vk-bootstrap:
- Select GPU with graphics + compute + present support
- Prefer discrete GPU
- Require Vulkan 1.3 features:
  - `dynamicRendering` (VK_KHR_dynamic_rendering promoted to core)
  - `synchronization2` (VK_KHR_synchronization2 promoted to core)
  - `maintenance4`
- Enable via `VkPhysicalDeviceVulkan13Features`
- Create VmaAllocator with the device
- Log selected GPU name, API version, driver version via `HVE_CORE_INFO_TAG("Vulkan", ...)`

Initially implement only:
- Constructor (device selection + VMA init)
- `WaitIdle()` → `vkDeviceWaitIdle()`
- Stub all other Create* methods to return nullptr (will be implemented in subsequent tasks)

Expose getters for Vulkan handles needed by other backend classes:
- `VkDevice GetDevice()`
- `VkPhysicalDevice GetPhysicalDevice()`
- `VkQueue GetGraphicsQueue()`
- `uint32_t GetGraphicsQueueFamily()`
- `VmaAllocator GetAllocator()`

- [ ] **Step 3: Create VMA implementation file**

Create `Engine/src/Renderer/Vulkan/VulkanVMA.cpp` that defines the VMA implementation:

```cpp
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

This must be in exactly one .cpp file.

- [ ] **Step 4: Verify compilation**

```bash
vendor/premake/premake5 gmake2 && make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

Expected: Compiles with Vulkan headers and vk-bootstrap.

- [ ] **Step 5: Commit**

```bash
git add Engine/src/Renderer/Vulkan/
git commit -m "feat: implement VulkanContext and VulkanDevice with vk-bootstrap"
```

---

### Task 4: Implement VulkanSwapchain

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanSwapchain.h`
- Create: `Engine/src/Renderer/Vulkan/VulkanSwapchain.cpp`

- [ ] **Step 1: Implement VulkanSwapchain**

Implements `RHISwapchain`. Uses vk-bootstrap's `vkb::SwapchainBuilder`.

Key members:
- `VkSwapchainKHR m_Swapchain`
- `std::vector<VkImage> m_Images`
- `std::vector<VkImageView> m_ImageViews`
- `VkFormat m_ImageFormat`
- `VkExtent2D m_Extent`
- `uint32_t m_CurrentImageIndex`
- Frame-in-flight sync: `VkSemaphore m_ImageAvailable[MAX_FRAMES]`, `VkSemaphore m_RenderFinished[MAX_FRAMES]`, `VkFence m_InFlight[MAX_FRAMES]`
- `uint32_t m_CurrentFrame = 0` (ping-pong between MAX_FRAMES=2)

Methods:
- Constructor: `vkb::SwapchainBuilder` → create swapchain, get images/views, create sync objects
- `AcquireNextImage()`: Wait on fence, `vkAcquireNextImageKHR`, return true/false (false = needs resize)
- `Present()`: `vkQueuePresentKHR`, advance `m_CurrentFrame`
- `Resize()`: `vkDeviceWaitIdle`, destroy old swapchain, rebuild
- Implement `GetCurrentImage()`, `GetWidth()`, `GetHeight()`

Note: `GetRenderPass()` and `GetCurrentFramebuffer()` will return nullptr initially — they require VulkanRenderPass/VulkanFramebuffer which come in Task 6.

- [ ] **Step 2: Commit**

```bash
git add Engine/src/Renderer/Vulkan/VulkanSwapchain.*
git commit -m "feat: implement VulkanSwapchain with frame-in-flight sync"
```

---

### Task 5: Implement VulkanBuffer, VulkanShader, VulkanCommandBuffer

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanBuffer.h/cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanShader.h/cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanCommandBuffer.h/cpp`

- [ ] **Step 1: Implement VulkanBuffer**

Implements `RHIBuffer`. Uses VMA for allocation.

```cpp
class VulkanBuffer : public RHIBuffer {
    VkBuffer m_Buffer;
    VmaAllocation m_Allocation;
    VmaAllocationInfo m_AllocInfo;
    BufferDesc m_Desc;
public:
    VulkanBuffer(VulkanDevice* device, const BufferDesc& desc, const void* initialData);
    ~VulkanBuffer() override; // vmaDestroyBuffer
    void SetData(const void* data, uint32_t size, uint32_t offset) override; // vmaMapMemory + memcpy + vmaUnmapMemory
    void* Map() override; // vmaMapMemory
    void Unmap() override; // vmaUnmapMemory
    VkBuffer GetVkBuffer() const { return m_Buffer; }
};
```

Buffer creation uses `vmaCreateBuffer` with appropriate `VkBufferCreateInfo` and `VmaAllocationCreateInfo`. Map `BufferUsage` flags to `VkBufferUsageFlagBits`. Map `MemoryAccess` to VMA usage flags (`VMA_MEMORY_USAGE_GPU_ONLY`, `VMA_MEMORY_USAGE_CPU_TO_GPU`, etc.).

For `GPU_Only` buffers with initial data: create a staging buffer, copy data, submit a one-shot command buffer to transfer, destroy staging buffer.

- [ ] **Step 2: Implement VulkanShader**

Implements `RHIShader`. Loads SPIR-V bytecode into a `VkShaderModule`.

```cpp
class VulkanShader : public RHIShader {
    VkShaderModule m_Module;
    ShaderStage m_Stage;
    std::string m_EntryPoint;
public:
    VulkanShader(VulkanDevice* device, const ShaderDesc& desc);
    ~VulkanShader() override; // vkDestroyShaderModule
    VkShaderModule GetModule() const { return m_Module; }
    VkShaderStageFlagBits GetVkStage() const;
    const char* GetEntryPoint() const { return m_EntryPoint.c_str(); }
};
```

Constructor calls `vkCreateShaderModule` with the SPIR-V bytes from `ShaderDesc`.

- [ ] **Step 3: Implement VulkanCommandBuffer**

Implements `RHICommandBuffer`. Wraps `VkCommandBuffer` + `VkCommandPool`.

Each method maps directly to a Vulkan command:
- `Begin()` → `vkBeginCommandBuffer`
- `End()` → `vkEndCommandBuffer`
- `BeginRenderPass()` → `vkCmdBeginRenderPass` (build `VkRenderPassBeginInfo` from args)
- `EndRenderPass()` → `vkCmdEndRenderPass`
- `BindPipeline()` → `vkCmdBindPipeline`
- `SetViewport()` → `vkCmdSetViewport`
- `SetScissor()` → `vkCmdSetScissor`
- `BindVertexBuffer()` → `vkCmdBindVertexBuffers`
- `BindIndexBuffer()` → `vkCmdBindIndexBuffer`
- `BindDescriptorSet()` → `vkCmdBindDescriptorSets`
- `PushConstants()` → `vkCmdPushConstants`
- `Draw()` → `vkCmdDraw`
- `DrawIndexed()` → `vkCmdDrawIndexed`
- `Dispatch()` → `vkCmdDispatch`
- `PipelineBarrier()` → `vkCmdPipelineBarrier` (simplified wrapper)
- `CopyBuffer()` → `vkCmdCopyBuffer`

Store a reference to VulkanDevice to access `VkDevice` and queue family.

- [ ] **Step 4: Wire up Create methods in VulkanDevice**

Implement `CreateBuffer`, `CreateShader`, `CreateCommandBuffer` in VulkanDevice to construct the Vulkan implementations.

- [ ] **Step 5: Verify compilation**

```bash
make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

- [ ] **Step 6: Commit**

```bash
git add Engine/src/Renderer/Vulkan/
git commit -m "feat: implement VulkanBuffer, VulkanShader, VulkanCommandBuffer"
```

---

### Task 6: Implement VulkanRenderPass, VulkanFramebuffer, VulkanPipeline

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanRenderPass.h/cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanFramebuffer.h/cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanPipeline.h/cpp`

**Note on dynamic rendering (Vulkan 1.3):** For most render passes, the Vulkan backend should use `vkCmdBeginRendering` / `vkCmdEndRendering` (dynamic rendering) instead of creating `VkRenderPass` + `VkFramebuffer` objects. The RHI `BeginRenderPass(desc, attachments, clear)` internally uses the dynamic rendering path. However, `VkRenderPass` objects are still needed for: (a) VkPipeline creation (requires a compatible render pass), and (b) ImGui's Vulkan backend. So VulkanRenderPass creates a VkRenderPass for compatibility but the actual rendering uses dynamic rendering.

- [ ] **Step 1: Implement VulkanRenderPass**

Implements `RHIRenderPass`. Creates a `VkRenderPass` for pipeline compatibility purposes only. The actual render pass begin/end in VulkanCommandBuffer uses `vkCmdBeginRendering` (Vulkan 1.3 dynamic rendering).

Constructor takes `RenderPassDesc`, creates `VkAttachmentDescription` array (color + optional depth), one subpass, subpass dependencies. Maps `ImageFormat` to `VkFormat`, `LoadOp`/`StoreOp` to Vulkan equivalents. Names the object via `SetDebugName`.

- [ ] **Step 2: Implement VulkanFramebuffer**

Implements `RHIFramebuffer`. Stores attachment `VulkanTexture` references and dimensions. With dynamic rendering, no `VkFramebuffer` object is needed for most passes — the attachment images are specified directly in `VkRenderingInfo`. However, create a `VkFramebuffer` for the swapchain pass (needed by ImGui).

- [ ] **Step 3: Implement VulkanPipeline**

Implements `RHIPipeline`. Wraps `VkPipeline` + `VkPipelineLayout`.

For graphics pipeline (`GraphicsPipelineDesc`):
- Build `VkPipelineShaderStageCreateInfo` array from vertex + fragment `VulkanShader`
- Build `VkPipelineVertexInputStateCreateInfo` from `VertexLayout`
- Build `VkPipelineInputAssemblyStateCreateInfo` (triangles)
- Build viewport/scissor as dynamic state
- Build `VkPipelineRasterizationStateCreateInfo` from `CullMode`
- Build `VkPipelineDepthStencilStateCreateInfo` from `DepthCompare`, `DepthWrite`
- Build `VkPipelineColorBlendAttachmentState` from `BlendMode`
- Build `VkPipelineMultisampleStateCreateInfo` (no MSAA initially)
- Create `VkPipelineLayout` from descriptor set layouts + push constant ranges
- Call `vkCreateGraphicsPipelines`

For compute pipeline (`ComputePipelineDesc`):
- Single compute shader stage
- Create layout + `vkCreateComputePipelines`

- [ ] **Step 4: Wire up Create methods in VulkanDevice**

Implement `CreateGraphicsPipeline`, `CreateComputePipeline`, `CreateRenderPass`, `CreateFramebuffer`.

- [ ] **Step 5: Complete VulkanSwapchain render pass and framebuffers**

Now that VulkanRenderPass and VulkanFramebuffer exist, update VulkanSwapchain to:
- Create a render pass for swapchain presentation (color attachment, load=Clear, store=Store)
- Create one VkFramebuffer per swapchain image
- Implement `GetRenderPass()` and `GetCurrentFramebuffer()`

- [ ] **Step 6: Commit**

```bash
git add Engine/src/Renderer/Vulkan/
git commit -m "feat: implement VulkanRenderPass, VulkanFramebuffer, VulkanPipeline"
```

---

### Task 7: Implement VulkanTexture and VulkanDescriptor

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanTexture.h/cpp`
- Create: `Engine/src/Renderer/Vulkan/VulkanDescriptor.h/cpp`

- [ ] **Step 1: Implement VulkanTexture**

Implements `RHITexture`. Wraps `VkImage` + `VkImageView` + `VkSampler` + `VmaAllocation`.

Constructor takes `TextureDesc` + optional initial data:
- `vmaCreateImage` for the VkImage with appropriate format, usage, tiling
- `vkCreateImageView` for the view (2D, Cube, or 2DArray based on TextureType)
- `vkCreateSampler` with linear filtering + repeat wrapping (can be customized later)
- If initial data: create staging buffer, copy via command buffer, transition image layout

Image layout transitions via `vkCmdPipelineBarrier` with `VkImageMemoryBarrier`:
- `UNDEFINED → TRANSFER_DST_OPTIMAL` (for upload)
- `TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` (for sampling)
- `UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL` (for depth textures)
- `UNDEFINED → COLOR_ATTACHMENT_OPTIMAL` (for render targets)

Expose: `VkImage GetVkImage()`, `VkImageView GetVkImageView()`, `VkSampler GetVkSampler()`, `VkImageLayout GetCurrentLayout()`.

- [ ] **Step 2: Implement VulkanDescriptor**

Three classes:

**VulkanDescriptorSetLayout** implements `RHIDescriptorSetLayout`:
- Constructor takes `DescriptorSetLayoutDesc`, creates `VkDescriptorSetLayout` from bindings
- Maps `DescriptorType` to `VkDescriptorType`

**VulkanDescriptorPool** (internal, not exposed via RHI):
- Manages a `VkDescriptorPool`
- Created by VulkanDevice with reasonable pool sizes

**VulkanDescriptorSet** implements `RHIDescriptorSet`:
- Wraps `VkDescriptorSet` allocated from the pool
- `VulkanDevice::UpdateDescriptorSet()` calls `vkUpdateDescriptorSets` with `VkWriteDescriptorSet` array built from `DescriptorWrite` list

- [ ] **Step 3: Wire up Create methods in VulkanDevice**

Implement `CreateTexture`, `CreateDescriptorSetLayout`, `AllocateDescriptorSet`, `UpdateDescriptorSet`.

Also implement `SubmitCommandBuffer` — submits to graphics queue with proper synchronization.

- [ ] **Step 4: Commit**

```bash
git add Engine/src/Renderer/Vulkan/
git commit -m "feat: implement VulkanTexture and VulkanDescriptor"
```

---

### Task 8: Render a triangle — proof of life

**Files:**
- Create: `Engine/src/Renderer/Vulkan/VulkanBootstrap.h/cpp` (temporary test harness)
- Create: `Editor/Resources/Shaders/triangle.vert` + `triangle.frag`
- Modify: `Engine/src/Core/Application.cpp` (temporarily wire up Vulkan instead of GL)

- [ ] **Step 1: Create minimal triangle shaders**

`triangle.vert`:
```glsl
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

`triangle.frag`:
```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

Compile both:
```bash
glslc Editor/Resources/Shaders/triangle.vert -o Editor/Resources/Shaders/triangle.vert.spv
glslc Editor/Resources/Shaders/triangle.frag -o Editor/Resources/Shaders/triangle.frag.spv
```

- [ ] **Step 2: Create VulkanBootstrap test harness**

A temporary class that:
1. Initializes VulkanContext + VulkanDevice + VulkanSwapchain
2. Creates a vertex buffer with a hardcoded triangle (3 vertices with position + color)
3. Loads the triangle SPIR-V shaders via `device->CreateShader()`
4. Creates a render pass and pipeline for the swapchain format
5. In a render loop: acquire image → begin cmd → begin render pass → bind pipeline → bind VBO → draw(3) → end render pass → end cmd → submit → present

This proves the entire Vulkan stack works end-to-end. It temporarily replaces the OpenGL rendering path.

- [ ] **Step 3: Wire into Application**

Temporarily modify `Application.cpp` to call VulkanBootstrap instead of the GL Renderer during the main loop. The GL Renderer code can be `#ifdef`'d out or commented out.

The GLFW window must NOT create an OpenGL context — add `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` before `glfwCreateWindow` in the Window implementation.

- [ ] **Step 4: Run and verify**

```bash
make config=debug Editor -j$(nproc) && cd Editor && ../bin/Debug-linux-x86_64/Editor/Editor
```

Expected: A window opens showing a colored triangle rendered by Vulkan.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: Vulkan triangle proof of life — entire RHI stack working"
```

---

## Phase 2: Port Renderer to RHI

### Task 9: Compile all existing GLSL shaders to SPIR-V

**Files:**
- Modify: All 24 files in `Editor/Resources/Shaders/*.vert/*.frag/*.comp/*.geo`
- Create: `compile_shaders.sh` script
- Modify: `Editor/premake5.lua` (add pre-build shader compilation)

- [ ] **Step 1: Update GLSL shaders for Vulkan compatibility**

For each shader, apply these changes:
- Ensure `#version 450` header
- Replace `uniform` blocks with `layout(set=N, binding=M) uniform BlockName { ... }`
- Replace `uniform sampler2D texName` with `layout(set=N, binding=M) uniform sampler2D texName`
- Replace SSBO syntax with `layout(set=N, binding=M) buffer BufferName { ... }`
- Keep `layout(location=N) in/out` for vertex attributes and varyings (these are correct for Vulkan)

Descriptor set assignments:
- **Set 0:** Global per-frame data (camera matrices, light data) — UBOs and SSBOs
- **Set 1:** Per-material data (textures, material properties) — samplers and UBO

- [ ] **Step 2: Create compile_shaders.sh**

```bash
#!/bin/bash
SHADER_DIR="Editor/Resources/Shaders"
for f in "$SHADER_DIR"/*.vert "$SHADER_DIR"/*.frag "$SHADER_DIR"/*.comp "$SHADER_DIR"/*.geo; do
    [ -f "$f" ] || continue
    echo "Compiling: $f"
    glslc "$f" -o "$f.spv" || exit 1
done
echo "All shaders compiled."
```

- [ ] **Step 3: Add shader compilation as Editor pre-build step**

Add to Editor premake5.lua prebuildcommands (before the existing dotnet build step):
```lua
'bash "%{wks.location}/compile_shaders.sh"'
```

- [ ] **Step 4: Compile and verify all shaders**

```bash
chmod +x compile_shaders.sh && ./compile_shaders.sh
```

Expected: All 24 shader files compile to `.spv` without errors.

- [ ] **Step 5: Commit**

```bash
git add Editor/Resources/Shaders/ compile_shaders.sh Editor/premake5.lua
git commit -m "feat: convert GLSL shaders to Vulkan-compatible, add SPIR-V compilation"
```

---

### Task 10: Port Renderer.cpp to RHI command buffers

**Files:**
- Rewrite: `Engine/src/Renderer/Renderer.h`
- Rewrite: `Engine/src/Renderer/Renderer.cpp`
- Rewrite: `Engine/src/Renderer/Material.h/cpp`
- Delete: `Engine/src/Renderer/Vulkan/VulkanBootstrap.h/cpp` (no longer needed)

This is the largest single task. The Renderer keeps its Forward+ pipeline logic but records to `RHICommandBuffer` instead of making GL calls.

- [ ] **Step 1: Rewrite Renderer.h**

Replace all GL types with RHI types:
- `RendererAPI m_RendererAPI` → remove entirely
- `GLuint m_WorkGroupsX/Y` → `uint32_t`
- `GLuint m_QuadVAO/VBO` → remove (use RHI buffer)
- `Ref<Framebuffer>` → `Ref<RHIFramebuffer>` + `Ref<RHIRenderPass>`
- `Ref<ShaderStorageBuffer>` → `Ref<RHIBuffer>`
- `Ref<UniformBuffer>` → `Ref<RHIBuffer>`
- `ShaderLibrary` → store `std::unordered_map<std::string, Ref<RHIPipeline>>` for graphics pipelines, separate map for compute

Add members:
- `RHIDevice* m_Device`
- `RHISwapchain* m_Swapchain`
- Per-frame descriptor sets for global data (camera, lights)
- Pipeline cache (map of pipeline configs → RHIPipeline)

- [ ] **Step 2: Rewrite Renderer.cpp — initialization**

Constructor:
1. Store device/swapchain pointers (passed in from Application)
2. Create render passes: depth, shadow, HDR, tonemap
3. Create framebuffers for each pass
4. Load all shaders from `.spv` files via `device->CreateShader()`
5. Create graphics pipelines for each pass (depth, shadow, forward, tonemap, skybox)
6. Create compute pipeline for light culling
7. Create descriptor set layouts and allocate descriptor sets
8. Create default textures (white, black, gray, blue) via `device->CreateTexture()`
9. Create UBOs and SSBOs via `device->CreateBuffer()`

- [ ] **Step 3: Rewrite Renderer.cpp — per-frame rendering**

Port each render pass to use command buffer recording:
- `DepthPrePass()` → `cmd->BeginRenderPass(depthPass, depthFB)` → bind depth pipeline → draw meshes → end
- Shadow pass → same pattern with shadow render pass
- `CullLights()` → `cmd->BindPipeline(lightCullCompute)` → bind descriptors → `cmd->Dispatch()` → barrier
- `ShadeAllObjects()` → `cmd->BeginRenderPass(hdrPass, hdrFB)` → for each mesh: bind material pipeline + descriptors → draw
- `ShadeHDR()` → `cmd->BeginRenderPass(tonemapPass, swapchainFB)` → bind tonemap pipeline → draw fullscreen triangle
- `DrawSkybox()` → bind skybox pipeline + cubemap descriptor → draw cube

- [ ] **Step 4: Rewrite Material.h/cpp**

Material becomes:
```cpp
class Material {
    Ref<RHIPipeline> m_Pipeline;
    Ref<RHIDescriptorSet> m_DescriptorSet;
    Ref<RHIBuffer> m_UniformBuffer;
    std::unordered_map<uint32_t, Ref<RHITexture>> m_Textures;

    void SetTexture(uint32_t binding, Ref<RHITexture> texture);
    void UpdateUniforms(const void* data, uint32_t size);
    void Bind(RHICommandBuffer* cmd, uint32_t setIndex);
};
```

`Bind()` calls `cmd->BindPipeline(m_Pipeline)` and `cmd->BindDescriptorSet(setIndex, m_DescriptorSet)`.

- [ ] **Step 5: Remove VulkanBootstrap**

Delete the temporary triangle test harness.

- [ ] **Step 6: Verify compilation**

```bash
make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: port Renderer to RHI command buffers, rewrite Material"
```

---

### Task 11: Port Texture, Buffer, Mesh wrappers

**Files:**
- Rewrite: `Engine/src/Renderer/Texture.h/cpp`
- Rewrite: `Engine/src/Renderer/Buffer.h/cpp`
- Modify: `Engine/src/Renderer/Mesh.h/cpp`
- Delete: `Engine/src/Renderer/VertexArray.h/cpp`
- Delete: `Engine/src/Renderer/UniformBuffer.h/cpp`

- [ ] **Step 1: Rewrite Texture.h/cpp**

`Texture2D` becomes a thin wrapper around `RHITexture`:

```cpp
class Texture2D {
    Ref<RHITexture> m_RHITexture;
    TextureDesc m_Desc;
public:
    Texture2D(const TextureSpecification& spec, Buffer data);
    static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer data);
    RHITexture* GetRHITexture() { return m_RHITexture.get(); }
    uint32_t GetWidth() { return m_Desc.Width; }
    uint32_t GetHeight() { return m_Desc.Height; }
    bool IsLoaded() { return m_RHITexture != nullptr; }
};
```

Similarly for `TextureCube`.

- [ ] **Step 2: Rewrite Buffer.h/cpp**

`VertexBuffer` and `IndexBuffer` become wrappers around `RHIBuffer`:

```cpp
class VertexBuffer {
    Ref<RHIBuffer> m_RHIBuffer;
    BufferLayout m_Layout;
public:
    static Ref<VertexBuffer> Create(float* vertices, uint32_t size);
    RHIBuffer* GetRHIBuffer() { return m_RHIBuffer.get(); }
    const BufferLayout& GetLayout() const { return m_Layout; }
};
```

`BufferLayout` and `ShaderDataType` stay as-is (backend-agnostic vertex attribute description).

Delete `VertexArray.h/cpp` — VAOs don't exist in Vulkan. Vertex input is described in the pipeline.

Delete `UniformBuffer.h/cpp` — replaced by `RHIBuffer` with `BufferUsage::Uniform`.

- [ ] **Step 3: Update Mesh.h**

Remove `Ref<VertexArray>` from `Submesh`. Replace with:
```cpp
struct Submesh {
    uint32_t Index, MaterialIndex;
    Ref<VertexBuffer> VBO;
    Ref<IndexBuffer> IBO;
    uint32_t IndexCount;
    glm::mat4 LocalTransform, WorldTransform;
    std::string MeshName;
    Math::BoundingBox Bounds;
};
```

- [ ] **Step 4: Update ModelImporter to create RHI buffers**

Read `Engine/src/Assets/ModelImporter.cpp`. Change vertex/index buffer creation to use the new VertexBuffer/IndexBuffer wrappers instead of VertexArray.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: port Texture, Buffer, Mesh to RHI wrappers, delete VertexArray/UniformBuffer"
```

---

## Phase 3: Delete OpenGL + Integrate

### Task 12: Switch window to Vulkan surface, switch ImGui to Vulkan backend

**Files:**
- Modify: `Engine/src/Platform/Linux/LinuxWindow.cpp`
- Modify: `Engine/src/Platform/Windows/WindowsWindow.cpp`
- Modify: `Engine/src/ImGui/ImGuiLayer.cpp`
- Delete: `Engine/src/ImGui/imgui_impl_opengl3.cpp`
- Delete: `Engine/src/ImGui/imgui_impl_opengl3.h`
- Delete: `Engine/src/ImGui/imgui_impl_opengl3_loader.h`
- Create: `Engine/src/ImGui/imgui_impl_vulkan.cpp` (from imgui repo)
- Create: `Engine/src/ImGui/imgui_impl_vulkan.h`
- Delete: `Engine/src/Renderer/RenderContext.h/cpp`

- [ ] **Step 1: Update LinuxWindow to not create GL context**

In `LinuxWindow::Init()`, add before `glfwCreateWindow`:
```cpp
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
```

Remove `m_Context = RenderContext::Create(m_Window)` and `m_Context->Init()`. Remove `m_Context->SwapBuffers()` from `OnUpdate()` (swapchain presentation is now handled by the Renderer).

Same changes for `WindowsWindow.cpp`.

- [ ] **Step 2: Add ImGui Vulkan backend**

Download `imgui_impl_vulkan.cpp` and `imgui_impl_vulkan.h` from the imgui repo (same version as the vendored imgui).

Delete `imgui_impl_opengl3.cpp`, `imgui_impl_opengl3.h`, `imgui_impl_opengl3_loader.h`.

- [ ] **Step 3: Update ImGuiLayer.cpp**

Replace:
```cpp
#include "imgui_impl_opengl3.h"
ImGui_ImplOpenGL3_Init("#version 410");
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
ImGui_ImplOpenGL3_Shutdown();
```

With Vulkan equivalents:
```cpp
#include "imgui_impl_vulkan.h"
// Init requires VulkanDevice info, render pass, descriptor pool
ImGui_ImplVulkan_Init(initInfo);
ImGui_ImplVulkan_NewFrame();
ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
ImGui_ImplVulkan_Shutdown();
```

The ImGui Vulkan init needs a `ImGui_ImplVulkan_InitInfo` struct with: Instance, PhysicalDevice, Device, QueueFamily, Queue, DescriptorPool, MinImageCount, ImageCount, RenderPass.

- [ ] **Step 4: Delete RenderContext.h/cpp**

No longer needed — Vulkan context is managed by VulkanContext.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: switch window to Vulkan surface, ImGui to Vulkan backend"
```

---

### Task 13: Remove OpenGL, update build system, final integration

**Files:**
- Delete: `Engine/src/Renderer/RendererAPI.h/cpp`
- Delete: `Engine/src/Renderer/Shader.h/cpp`
- Delete: `Engine/src/Renderer/ShaderProgram.h/cpp`
- Delete: `Engine/vendor/Glad/` (entire directory)
- Modify: `Dependencies.lua` (remove Glad)
- Modify: `Engine/premake5.lua` (remove Glad)
- Modify: `Editor/premake5.lua` (remove Glad)
- Modify: `EditorLauncher/premake5.lua` (remove Glad)
- Modify: `premake5.lua` (remove Glad from dependencies group)
- Modify: `Engine/src/pch.h` (remove `#include <glad/gl.h>`)
- Modify: `Engine/src/Core/Application.cpp` (wire up Vulkan device/swapchain)

- [ ] **Step 1: Delete all OpenGL-specific files**

```bash
rm -rf Engine/vendor/Glad
rm Engine/src/Renderer/RendererAPI.h Engine/src/Renderer/RendererAPI.cpp
rm Engine/src/Renderer/Shader.h Engine/src/Renderer/Shader.cpp
rm Engine/src/Renderer/ShaderProgram.h Engine/src/Renderer/ShaderProgram.cpp
rm Engine/src/Renderer/RenderContext.h Engine/src/Renderer/RenderContext.cpp
```

- [ ] **Step 2: Update all premake files**

Remove all references to Glad:
- `Dependencies.lua`: remove `IncludeDir["Glad"]`
- `premake5.lua`: remove `include "Engine/vendor/Glad"` from Dependencies group
- `Engine/premake5.lua`: remove `"%{IncludeDir.Glad}"` from includedirs, remove `"Glad"` from links
- `Editor/premake5.lua`: remove `"Glad"` from links
- `EditorLauncher/premake5.lua`: remove `"Glad"` from links

Add Vulkan to links for Editor and EditorLauncher (Linux: `"vulkan"`, Windows: `"vulkan-1"`).

- [ ] **Step 3: Update pch.h**

Remove any `#include <glad/gl.h>`.

- [ ] **Step 4: Update Application.cpp**

Wire up Vulkan initialization:
```cpp
// In Application constructor, replace Renderer::CreateRenderer() with:
VulkanContext::Init("Helios", true); // true = validation in debug
auto surface = VulkanContext::CreateSurface(m_Window->GetNativeWindow());
auto device = CreateRef<VulkanDevice>(surface);
auto swapchain = CreateRef<VulkanSwapchain>(device.get(), surface, m_Window->GetWidth(), m_Window->GetHeight());
Renderer::CreateRenderer(device.get(), swapchain.get());
```

In the main loop, replace `m_Window->OnUpdate()` (which called SwapBuffers) with just `glfwPollEvents()`. Swapchain present is handled by the Renderer's EndFrame.

- [ ] **Step 5: Fix all remaining compilation errors**

Grep for any remaining `glad`, `glCreate`, `glBind`, `glDraw`, `GL_` references and fix them.

```bash
grep -r "glad/gl.h\|glCreate\|glBind\|glDraw\|GL_TEXTURE\|GL_FRAMEBUFFER" Engine/src/ --include="*.cpp" --include="*.h" | grep -v "Vulkan/" | head -20
```

Fix each reference.

- [ ] **Step 6: Full build**

```bash
vendor/premake/premake5 gmake2 && make config=debug -j$(nproc) 2>&1 | tail -10
```

Expected: Clean build with no OpenGL references.

- [ ] **Step 7: Run and verify**

```bash
cd Editor && ../bin/Debug-linux-x86_64/Editor/Editor
```

Expected: Editor opens with Vulkan rendering. The full Forward+ pipeline renders: skybox, meshes with PBR materials, shadows, HDR tonemapping, ImGui overlay.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "feat: remove all OpenGL code, complete Vulkan migration"
```

---

### Task 14: Update generate_linux_projects.sh and cleanup

**Files:**
- Modify: `generate_linux_projects.sh`
- Modify: `generate_win_projects.bat`

- [ ] **Step 1: Add Vulkan SDK check to prerequisites**

In `generate_linux_projects.sh`, add to prerequisites check:
```bash
if ! pkg-config --exists vulkan 2>/dev/null; then
    log_error "Vulkan SDK not found. Install vulkan-devel (e.g. 'sudo pacman -S vulkan-headers vulkan-tools')"
    MISSING=1
fi
if ! command -v glslc &>/dev/null; then
    log_error "glslc not found. Install shaderc (e.g. 'sudo pacman -S shaderc')"
    MISSING=1
fi
```

- [ ] **Step 2: Add shader compilation to the generate script**

After premake generation, call:
```bash
log_info "Compiling shaders..."
bash "$SCRIPT_DIR/compile_shaders.sh"
```

- [ ] **Step 3: Remove Glad download/build references**

Remove any Glad-related steps from both scripts.

- [ ] **Step 4: Commit**

```bash
git add generate_linux_projects.sh generate_win_projects.bat
git commit -m "build: update project generation scripts for Vulkan (add SDK check, shader compilation)"
```
