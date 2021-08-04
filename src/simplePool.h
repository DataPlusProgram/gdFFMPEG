#ifndef POOL_HPP
#define POOL_HPP
#include <vector>
#include "ImageFrame.h"


struct PoolEntry
{
public:
	ImageFrame data;
	int id;
	bool free = true;
};

class SimplePool
{
public:
	godot::Vector2 dimensions;
	std::vector<PoolEntry> pool;
	PoolEntry fetch();
	void free(int id);

	SimplePool(godot::Vector2 dimensions);
};

#endif