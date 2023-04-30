#pragma once

#include "HVEDevice.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace hve
{
	class HVEDescriptorSetLayout
	{
    public:
        class Builder {
        public:
            Builder(HVEDevice& hveDevice) : hveDevice{ hveDevice } {}

            Builder& addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,
                VkShaderStageFlags stageFlags,
                uint32_t count = 1);
            std::unique_ptr<HVEDescriptorSetLayout> build() const;

        private:
            HVEDevice& hveDevice;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
        };
        HVEDescriptorSetLayout(
            HVEDevice& hveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~HVEDescriptorSetLayout();
        HVEDescriptorSetLayout(const HVEDescriptorSetLayout&) = delete;
        HVEDescriptorSetLayout& operator=(const HVEDescriptorSetLayout&) = delete;

        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    private:
        HVEDevice& hveDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        friend class HVEDescriptorWriter;
	};

    class HVEDescriptorPool {
    public:
        class Builder {
        public:
            Builder(HVEDevice& hveDevice) : hveDevice{ hveDevice } {}

            Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);
            Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
            Builder& setMaxSets(uint32_t count);
            std::unique_ptr<HVEDescriptorPool> build() const;

        private:
            HVEDevice& hveDevice;
            std::vector<VkDescriptorPoolSize> poolSizes{};
            uint32_t maxSets = 1000;
            VkDescriptorPoolCreateFlags poolFlags = 0;
        };

        HVEDescriptorPool(
            HVEDevice& hveDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize>& poolSizes);
        ~HVEDescriptorPool();
        HVEDescriptorPool(const HVEDescriptorPool&) = delete;
        HVEDescriptorPool& operator=(const HVEDescriptorPool&) = delete;

        bool allocateDescriptor(
            const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

        void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

        void resetPool();

    private:
        HVEDevice& hveDevice;
        VkDescriptorPool descriptorPool;

        friend class HVEDescriptorWriter;
    };

    class HVEDescriptorWriter {
    public:
        HVEDescriptorWriter(HVEDescriptorSetLayout& setLayout, HVEDescriptorPool& pool);

        HVEDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        HVEDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

        bool build(VkDescriptorSet& set);
        void overwrite(VkDescriptorSet& set);

    private:
        HVEDescriptorSetLayout& setLayout;
        HVEDescriptorPool& pool;
        std::vector<VkWriteDescriptorSet> writes;
    };
}