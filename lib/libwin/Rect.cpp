#include <win/Rect.h>
#include <stdio.h>

namespace os {
namespace gui {

bool Rect::Intersects(const Rect &rect) const
{
	return max(left, rect.left) <= min(right, rect.right)
		&& max(top, rect.top) <= min(bottom, rect.bottom);
}

void Rect::Dump() const
{
	printf("Rect (%d, %d, %d, %d)", left, top, right, bottom);
}

bool Rect::Contains(int x, int y) const
{
	return (x >= left && x <= right && y >= top && y <= bottom);
}

bool Rect::Contains(const Rect &rect) const
{
	return rect.left >= left && rect.right <= right
		&& rect.top >= top && rect.bottom <= bottom;
}

Rect& Rect::Intersect(const Rect &rect)
{
	left = max(left, rect.left);
	right = min(right, rect.right);
	top = max(top, rect.top);
	bottom = min(bottom, rect.bottom);
	return *this;
}

}
}

