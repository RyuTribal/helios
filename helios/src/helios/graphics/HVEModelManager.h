#pragma once
#include <mutex>
#include <unordered_map>
#include "HVEModel.h"


#include "helios/enums/HVEShapes.h"


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
		std::mutex model_lock;
		HVEDevice& device;
		std::unordered_map<Shapes, std::unique_ptr<HVEModel>> models{};

	};
}
