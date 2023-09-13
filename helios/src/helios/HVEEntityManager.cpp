#include "hvepch.h"

#include "helios/HVEEntityManager.h"

namespace hve {
	int HVEEntityManager::currentId = 0;

	int HVEEntityManager::createEntity()
	{
		std::lock_guard<std::mutex> lock(entity_lock);
		int newId = currentId++;
		activeEntities.insert(newId);
		return newId;
	}

	void HVEEntityManager::deleteEntity(int enitityId)
	{
		std::lock_guard<std::mutex> lock(entity_lock);
		activeEntities.erase(enitityId);
	}
}
