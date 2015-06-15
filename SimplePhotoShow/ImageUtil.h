#pragma once
#include <tuple>
#include <cmath>

template <typename T>
inline int RoundToNearest(T x)
{
	return static_cast<int>(std::floor(x + T(0.5)));
}

template <typename T>
std::tuple<T, T> ScaleToFit(T srcWidth, T srcHeight, T destWidth, T destHeight)
{
	if (srcWidth > T(0) && srcHeight > T(0))
	{
		T rw = destHeight * srcWidth / srcHeight;
		if (rw <= destWidth)
		{
			return std::make_tuple(rw, destHeight);
		}
		else
		{
			return std::make_tuple(destWidth, srcHeight / srcWidth * destWidth);
		}
	}
	return std::make_tuple(T(0), T(0));
}

inline std::tuple<int, int> ScaleToFit(int srcWidth, int srcHeight, int destWidth, int destHeight)
{
	if (srcWidth > 0 && srcHeight > 0)
	{
		int rw = RoundToNearest(destHeight * float(srcWidth) / srcHeight);
		if (rw <= destWidth)
		{
			return std::make_tuple(rw, destHeight);
		}
		else
		{
			return std::make_tuple(destWidth, RoundToNearest(srcHeight / srcWidth * destWidth));
		}
	}
	return std::make_tuple(0, 0);
}
