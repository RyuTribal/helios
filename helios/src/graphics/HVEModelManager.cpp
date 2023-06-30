#include "graphics/HVEModelManager.hpp"

#include <iostream>

#include "primitives/HVEShapes.hpp"

namespace hve
{
	void HVEModelManager::createModel(Shapes shape)
	{
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
		if (models.find(shape) == models.end())
		{
			return nullptr;
		}

		return models.at(shape).get();
	}
	void HVEModelManager::buildAllInstances()
	{
		for (auto i = models.begin(); i != models.end(); i++)
		{
			if (!i->second->getInstanceBuilt()) {
				i->second->buildCurrentInstances();
			}
		}
	}

	void HVEModelManager::clearAllInstances()
	{
		for (auto i = models.begin(); i != models.end(); i++)
		{
			i->second->clearInstances();
		}
	}
	void HVEModelManager::updateFrameIndex(int frameIndex)
	{
		for (auto i = models.begin(); i != models.end(); i++)
		{
			i->second->setCurrentFrame(frameIndex);
		}
	}
}
