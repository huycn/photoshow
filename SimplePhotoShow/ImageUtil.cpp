#include "ImageUtil.h"

std::tuple<int, int> ScaleToFit(int srcWidth, int srcHeight, int destWidth, int destHeight)
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
