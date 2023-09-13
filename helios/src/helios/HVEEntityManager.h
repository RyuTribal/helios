#pragma once
#include <mutex>
#include <unordered_set>


namespace hve
{
	class HVEEntityManager
	{
	public:
		int createEntity();
		void deleteEntity(int enitityId);
	private:
		std::mutex entity_lock;
		static int currentId;
		std::unordered_set<int> activeEntities;
	};
}
