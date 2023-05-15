#include "HVEEntityManager.hpp"

namespace hve {
	int HVEEntityManager::currentId = 0;

	int HVEEntityManager::createEntity()
	{
		int newId = currentId++;
		activeEntities.insert(newId);
		return newId;
	}

	void HVEEntityManager::deleteEntity(int enitityId)
	{
		activeEntities.erase(enitityId);
	}
}
