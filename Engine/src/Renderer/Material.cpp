#include "pch.h"
#include "Material.h"

namespace Engine {

    void Material::Set(const std::string& name, float value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;

        // Update material data struct for known uniforms
        if (name.find("Roughness") != std::string::npos) m_MaterialData.Roughness = value;
        else if (name.find("Metalness") != std::string::npos) m_MaterialData.Metalness = value;
        else if (name.find("Emission") != std::string::npos) m_MaterialData.Emission = value;
    }

    void Material::Set(const std::string& name, int value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, uint32_t value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, bool value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;

        if (name.find("UseNormalMap") != std::string::npos)
            m_MaterialData.UseNormalMap = value ? 1 : 0;
    }

    void Material::Set(const std::string& name, const glm::ivec2& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::ivec3& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::ivec4& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::vec2& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::vec3& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;

        if (name.find("AlbedoColor") != std::string::npos)
            m_MaterialData.AlbedoColor = value;
    }

    void Material::Set(const std::string& name, const glm::vec4& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::mat3& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const glm::mat4& value)
    {
        m_Uniforms[name] = value;
        m_Dirty = true;
    }

    void Material::Set(const std::string& name, const Ref<Texture2D>& texture, uint32_t slot)
    {
        m_Textures[slot] = texture;
        m_Dirty = true;
    }

    void Material::UpdateGPUData(RHIDevice* device)
    {
        if (!m_Dirty || !m_MaterialUBO || !m_DescriptorSet || !device)
            return;

        // Upload material uniform data
        m_MaterialUBO->SetData(&m_MaterialData, sizeof(MaterialData));

        // Build descriptor writes for all texture bindings
        std::vector<DescriptorWrite> writes;

        // Binding 0: MaterialUBO
        DescriptorWrite uboWrite;
        uboWrite.Binding = 0;
        uboWrite.Type = DescriptorType::UniformBuffer;
        uboWrite.Buffer = m_MaterialUBO.get();
        uboWrite.Range = sizeof(MaterialData);
        writes.push_back(uboWrite);

        // Texture bindings (1-11)
        for (auto& [slot, texture] : m_Textures)
        {
            if (texture && texture->GetRHITexture())
            {
                DescriptorWrite texWrite;
                texWrite.Binding = slot;
                texWrite.Type = DescriptorType::CombinedImageSampler;
                texWrite.Texture = texture->GetRHITexture();
                writes.push_back(texWrite);
            }
        }

        device->UpdateDescriptorSet(m_DescriptorSet.get(), writes);
        m_Dirty = false;
    }

}
