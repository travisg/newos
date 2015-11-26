#ifndef _WIN_RECT_H
#define _WIN_RECT_H

#include <stdlib.h>

#include <win/Point.h>

namespace os {
namespace gui {

class Rect {
public:
    inline Rect();
    inline Rect(int left, int top, int right, int bottom);
    inline Rect(const Point&, const Point&);
    inline void SetTo(int left, int top, int right, int bottom);
    inline void SetTo(const Point&, const Point&);
    inline void SetToSize(int left, int top, int width, int height);
    inline Rect& InsetBy(int, int);
    inline Rect& OffsetBy(int, int);
    inline Rect& OffsetTo(int, int);
    bool Contains(int, int) const;
    bool Contains(const Rect&) const;
    bool Contains(const Point&) const;
    bool Intersects(const Rect&) const;
    Rect& Intersect(const Rect&);
    inline bool Valid() const;
    inline int Width() const;
    inline int Height() const;
    inline Rect Bounds() const;

    void Dump() const;

    int left;
    int top;
    int right;
    int bottom;
};

inline Rect::Rect()
    :   left(0),
        top(0),
        right(0),
        bottom(0)
{
}

inline Rect::Rect(int l, int t, int r, int b)
    :   left(l),
        top(t),
        right(r),
        bottom(b)
{
}

inline Rect::Rect(const Point &ul, const Point &lr)
    :   left(ul.x),
        top(ul.y),
        right(lr.x),
        bottom(lr.y)
{
}

inline void Rect::SetTo(int l, int t, int r, int b)
{
    left = l;
    top = t;
    right = r;
    bottom = b;
}

inline void Rect::SetTo(const Point &ul, const Point &lr)
{
    left = ul.x;
    top = ul.y;
    right = lr.x;
    bottom = lr.y;
}

inline void Rect::SetToSize(int l, int t, int w, int h)
{
    left = l;
    top = t;
    right = l + w - 1;
    bottom = t + h - 1;
}

inline Rect& Rect::InsetBy(int h, int v)
{
    left += h;
    right -= h;
    top += v;
    bottom -= v;
    return *this;
}

inline Rect& Rect::OffsetBy(int h, int v)
{
    left += h;
    right += h;
    top += v;
    bottom += v;
    return *this;
}

inline Rect& Rect::OffsetTo(int h, int v)
{
    right += (h - left);
    bottom += (v - top);
    left = h;
    top = v;
    return *this;
}

inline bool Rect::Valid() const
{
    return right >= left && bottom >= top;
}

inline int Rect::Width() const
{
    return right - left + 1;
}

inline int Rect::Height() const
{
    return bottom - top + 1;
}

inline Rect Rect::Bounds() const
{
    return Rect(0, 0, right - left, bottom - top);
}

}
}

#endif
