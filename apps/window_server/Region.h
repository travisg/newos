#ifndef _REGION_H
#define _REGION_H

#include <win/Rect.h>
#include "assert.h"

const int COORD_MAX = 0x7ffffff0;

class Region {
public:

	Region();
	~Region();
	Region(const Region&);
	Region& Clear();
	Region& Include(const Rect&);
	Region& Include(const Region&);
	Region& Exclude(const Rect&);
	Region& Exclude(const Region&);
	Region& Invert();
	Region& Intersect(const Region&);
	bool Intersects(const Rect&) const;
	Region& Translate(int x, int y);
	Region& ConstrainTo(const Rect&);
	Region& SetTo(const Region&);
	Region& operator=(const Region&);

	const bool FindRect(int x, int y, Rect &outRect) const;

	Rect Bounds() const;
	inline bool Valid() const;
	inline int CountRects() const;
	const Rect& RectAt(int index) const;

	void Dump() const;

private:

	void AddRect(const Rect&);
	void RemoveRect(int index);
	void Consolidate();

	// Assumes there are no overlaps
	void AddRegionRects(const Region&);

	void AllocSpace(int numRects);

	void BeginOperation();
	void EndOperation();

	Rect *fRects;
	int fNumRects;
	int fOpLevel;
};

inline int Region::CountRects() const
{
	return fNumRects;
}

inline const Rect& Region::RectAt(int index) const
{
	ASSERT(index < fNumRects);
	return fRects[index];
}


inline bool Region::Valid() const
{
	return fNumRects > 0;
}


#endif
