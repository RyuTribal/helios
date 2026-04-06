#include "pch.h"
#include "Buffer.h"

namespace Engine {

    // Defined in Renderer.cpp
    extern RHIDevice* GetRendererDevice();

    // ---- VertexBuffer ----

    Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
    {
        return CreateRef<VertexBuffer>(size);
    }

    Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
    {
        return CreateRef<VertexBuffer>(vertices, size);
    }

    VertexBuffer::VertexBuffer(uint32_t size)
    {
        RHIDevice* device = GetRendererDevice();
        if (!device)
        {
            HVE_CORE_ERROR_TAG("Buffer", "VertexBuffer::Create called before Renderer is initialized");
            return;
        }

        BufferDesc desc;
        desc.Size = size;
        desc.Usage = BufferUsage::Vertex | BufferUsage::Transfer;
        desc.Access = MemoryAccess::CPU_to_GPU;
        desc.DebugName = "DynamicVertexBuffer";
        m_Buffer = device->CreateBuffer(desc);
    }

    VertexBuffer::VertexBuffer(float* vertices, uint32_t size)
    {
        RHIDevice* device = GetRendererDevice();
        if (!device)
        {
            HVE_CORE_ERROR_TAG("Buffer", "VertexBuffer::Create called before Renderer is initialized");
            return;
        }

        BufferDesc desc;
        desc.Size = size;
        desc.Usage = BufferUsage::Vertex | BufferUsage::Transfer;
        desc.Access = MemoryAccess::GPU_Only;
        desc.DebugName = "StaticVertexBuffer";
        m_Buffer = device->CreateBuffer(desc, vertices);
    }

    VertexBuffer::~VertexBuffer()
    {
    }

    void VertexBuffer::SetData(const void* data, uint32_t size)
    {
        if (m_Buffer)
            m_Buffer->SetData(data, size);
    }

    // ---- IndexBuffer ----

    Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
    {
        return CreateRef<IndexBuffer>(indices, count);
    }

    IndexBuffer::IndexBuffer(uint32_t* indices, uint32_t count)
        : m_Count(count)
    {
        RHIDevice* device = GetRendererDevice();
        if (!device)
        {
            HVE_CORE_ERROR_TAG("Buffer", "IndexBuffer::Create called before Renderer is initialized");
            return;
        }

        BufferDesc desc;
        desc.Size = count * sizeof(uint32_t);
        desc.Usage = BufferUsage::Index | BufferUsage::Transfer;
        desc.Access = MemoryAccess::GPU_Only;
        desc.DebugName = "IndexBuffer";
        m_Buffer = device->CreateBuffer(desc, indices);
    }

    IndexBuffer::~IndexBuffer()
    {
    }

}
