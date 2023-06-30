#pragma once
#include <unordered_map>
#include "HVEModel.hpp"
#include "enums/HVEShapes.h"


namespace hve
{
	class HVEModelManager
	{
	public:
		HVEModelManager(HVEDevice& device) : device{device}{}
		void createModel(Shapes shape);
		std::unordered_map<Shapes, std::unique_ptr<HVEModel>>* getMap() { return &models; }
		HVEModel* getModelByShape(Shapes shape);
		void buildAllInstances();
		void clearAllInstances();
		void updateFrameIndex(int frameIndex);
	private:
		HVEDevice& device;
		std::unordered_map<Shapes, std::unique_ptr<HVEModel>> models{};

	};
}
