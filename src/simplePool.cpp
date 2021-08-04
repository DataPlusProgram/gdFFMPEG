#include "simplePool.h"

PoolEntry SimplePool::fetch()
{
	for (auto i=0; i < pool.size(); i++)
	{
		if (pool[i].free == true)
		{
			pool[i].free = false;
			return pool[i];
		}
	}

	//nothing free in pool so create new entry

	PoolEntry newEntry;
	//newEntry.data = new unsigned char[frameSize];
	godot::Ref<godot::Image> image;
	image.instance();
	image->create(dimensions.x, dimensions.y, false, 5);
	
	ImageFrame imageFrame;
	imageFrame.img = image;
	
	newEntry.free = false;
	newEntry.id = pool.size();
	imageFrame.poolId = pool.size();
	newEntry.data = imageFrame;
	
	pool.push_back(newEntry);

	return newEntry;
}

void SimplePool::free(int id)
{
	pool[id].free = true;
}

SimplePool::SimplePool(godot::Vector2 dimensions)
{
	this->dimensions = dimensions;
}
