#ifndef _WIN_POINT_H
#define _WIN_POINT_H

namespace os {
namespace gui {

class Point {
public:
	inline Point();
	inline Point(int x, int y);
	inline void SetTo(int x, int y);
	inline void SetTo(const Point&);

	inline void OffsetBy(int x, int y);

	int x;
	int y;
};

inline Point::Point()
	:	x(0),
		y(0)
{
}

inline Point::Point(int in_x, int in_y)
	:	x(in_x),
		y(in_y)
{
}

inline void Point::SetTo(int in_x, int in_y)
{
	x = in_x;
	y = in_y;
}

inline void Point::SetTo(const Point &in)
{
	x = in.x;
	y = in.y;
}

inline void Point::OffsetBy(int h, int v)
{
	x += h;
	y += v;
}

}
}

#endif
