#pragma once
#include <Godot.hpp>
#include <Image.hpp>

struct ImageFrame
{
	godot::Ref<godot::Image> img;
	double timeStamp;
	int poolId;
};