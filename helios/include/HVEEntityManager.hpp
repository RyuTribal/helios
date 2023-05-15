#pragma once
#include <unordered_set>


namespace hve
{
	class HVEEntityManager
	{
	public:
		int createEntity();
		void deleteEntity(int enitityId);
	private:
		static int currentId;
		std::unordered_set<int> activeEntities;
	};
}
