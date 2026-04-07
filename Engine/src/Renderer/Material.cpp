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

    void Material::UpdateGPUData(RHIDevice* device, RHITexture* default2D,
                                   RHITexture* defaultCube, RHITexture* defaultArray,
                                   RHITexture* irradianceTex,
                                   RHITexture* prefilterTex,
                                   RHITexture* brdfTex)
    {
        if (!m_Dirty || !m_MaterialUBO || !m_DescriptorSet || !device)
            return;

        // Upload material uniform data
        m_MaterialUBO->SetData(&m_MaterialData, sizeof(MaterialData));

        // Build descriptor writes — ALL bindings must be valid to prevent GPU hangs.
        // Sampler types must match the shader declarations:
        //   1-9:    sampler2D      → default2D
        //   10:     samplerCube    → irradianceTex (or defaultCube)
        //   11:     samplerCube    → prefilterTex  (or defaultCube)
        //   12:     sampler2D      → brdfTex       (or default2D)
        //   13:     sampler2DArray → defaultArray
        std::vector<DescriptorWrite> writes;

        DescriptorWrite uboWrite;
        uboWrite.Binding = 0;
        uboWrite.Type = DescriptorType::UniformBuffer;
        uboWrite.Buffer = m_MaterialUBO.get();
        uboWrite.Range = sizeof(MaterialData);
        writes.push_back(uboWrite);

        for (uint32_t slot = 1; slot <= 13; slot++)
        {
            DescriptorWrite texWrite;
            texWrite.Binding = slot;
            texWrite.Type = DescriptorType::CombinedImageSampler;

            auto it = m_Textures.find(slot);
            if (it != m_Textures.end() && it->second && it->second->GetRHITexture())
            {
                texWrite.Texture = it->second->GetRHITexture();
            }
            else
            {
                // Pick default matching the shader's sampler type
                if (slot == 10)
                    texWrite.Texture = irradianceTex ? irradianceTex : defaultCube;
                else if (slot == 11)
                    texWrite.Texture = prefilterTex ? prefilterTex : defaultCube;
                else if (slot == 12)
                    texWrite.Texture = brdfTex ? brdfTex : default2D;
                else if (slot == 13)
                    texWrite.Texture = defaultArray;
                else
                    texWrite.Texture = default2D;
            }
            writes.push_back(texWrite);
        }

        device->UpdateDescriptorSet(m_DescriptorSet.get(), writes);
        m_Dirty = false;
    }

}
