#pragma once
#include <tuple>
#include <cmath>

template <typename T>
inline int RoundToNearest(T x)
{
	return static_cast<int>(std::floor(x + T(0.5)));
}

std::tuple<int, int> ScaleToFit(int srcWidth, int srcHeight, int destWidth, int destHeight);
