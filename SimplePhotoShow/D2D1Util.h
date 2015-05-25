#pragma once
#include <d2d1helper.h>

namespace D2D1 {

	template <typename T>
	void OffsetRect(typename TypeTraits<T>::Rect& rect, T dx, T dy)
	{
		rect.left += dx;
		rect.right += dx;
		rect.top += dy;
		rect.bottom += dy;
	}

}