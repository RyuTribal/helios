#include "hvepch.h"
#include "helios/graphics/HVEModelManager.h"



#include "helios/primitives/HVEShapes.h"

namespace hve
{
	void HVEModelManager::createModel(Shapes shape)
	{
		std::lock_guard<std::mutex> lock(model_lock);
		if (getModelByShape(shape) == nullptr)
		{
			switch (shape)
			{
			case Shapes::Quad:
				models.insert({ shape, HVEShapes::drawQuad(device) });
				break;
			default:
				models.insert({ shape, HVEShapes::drawQuad(device) });
			}
		}

	}
	HVEModel* HVEModelManager::getModelByShape(Shapes shape)
	{
		std::lock_guard<std::mutex> lock(model_lock);
		if (models.find(shape) == models.end())
		{
			return nullptr;
		}

		return models.at(shape).get();
	}
	void HVEModelManager::buildAllInstances()
	{
		std::lock_guard<std::mutex> lock(model_lock);
		for (auto i = models.begin(); i != models.end(); i++)
		{
			if (!i->second->getInstanceBuilt()) {
				i->second->buildCurrentInstances();
			}
		}
	}

	void HVEModelManager::clearAllInstances()
	{
		std::lock_guard<std::mutex> lock(model_lock);
		for (auto i = models.begin(); i != models.end(); i++)
		{
			i->second->clearInstances();
		}
	}
	void HVEModelManager::updateFrameIndex(int frameIndex)
	{
		std::lock_guard<std::mutex> lock(model_lock);
		for (auto i = models.begin(); i != models.end(); i++)
		{
			i->second->setCurrentFrame(frameIndex);
		}
	}
}
