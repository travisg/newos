#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Region.h"
#include "util.h"

const int kRectAllocSize = 16;

Region::Region()
	:	fRects(0),
		fNumRects(0),
		fOpLevel(0)
{
}

Region::Region(const Region &copyRegion)
	:	fRects(0),
		fNumRects(0),
		fOpLevel(0)
{
	SetTo(copyRegion);
}

Region::~Region()
{
	free(fRects);
}

Region& Region::SetTo(const Region &copyRegion)
{
	AllocSpace(copyRegion.fNumRects);
	fNumRects = copyRegion.fNumRects;
	memcpy(fRects, copyRegion.fRects, fNumRects * sizeof(Rect));
	return *this;
}

Region& Region::operator=(const Region &copyRegion)
{
	return SetTo(copyRegion);
}

Rect Region::Bounds() const
{
	Rect bounds(COORD_MAX, COORD_MAX, -COORD_MAX, -COORD_MAX);
	for (int i = 0; i < fNumRects; i++) {
		const Rect &rect = RectAt(i);
		bounds.left = min(bounds.left, rect.left);
		bounds.right = max(bounds.right, rect.right);
		bounds.top = min(bounds.top, rect.top);
		bounds.bottom = max(bounds.bottom, rect.bottom);
	}

	return bounds;
}

Region& Region::Include(const Rect &rect)
{
	Region temp;

	BeginOperation();
	// Make sure that there are no overlaps
	temp.AddRect(rect);
	for (int i = 0; i < fNumRects; i++)
		temp.Exclude(RectAt(i));

	AddRegionRects(temp);
	EndOperation();
	return *this;
}

Region& Region::Include(const Region &region)
{
	BeginOperation();
	for (int i = 0; i < region.CountRects(); i++)
		Include(region.RectAt(i));
	EndOperation();

	return *this;
}

#define SPLIT_TEST(rect1, rect2)											\
	(max(rect1.left, rect2.left) < min(rect1.right, rect2.right)			\
		&& max(rect1.top, rect2.top) < min(rect1.bottom, rect2.bottom))

Region& Region::Exclude(const Rect &excludeRect)
{
	BeginOperation();
	int index = 0;
	int rectsToCheck = fNumRects;
	while (index < rectsToCheck) {
		Rect &clipRect = fRects[index];

		if (!excludeRect.Intersects(clipRect)) {
			index++;
			continue;
		}

		// This clip rect intersects the excluded rect, and could be divided into
		// as many as eight pieces.  Test for each case.  Note that none of these
		// rectangles overlap!!!!
		Rect quad1(clipRect.left, clipRect.top, excludeRect.left - 1, excludeRect.top - 1);
		if (SPLIT_TEST(clipRect, quad1)) {
			quad1.Intersect(clipRect);
			AddRect(quad1);
		}

		Rect quad2(excludeRect.left, clipRect.top, excludeRect.right, excludeRect.top - 1);
		if (SPLIT_TEST(clipRect, quad2)) {
			quad2.Intersect(clipRect);
			AddRect(quad2);
		}

		Rect quad3(excludeRect.right + 1, clipRect.top, clipRect.right, excludeRect.top - 1);
		if (SPLIT_TEST(clipRect, quad3)) {
			quad3.Intersect(clipRect);
			AddRect(quad3);
		}

		Rect quad4(clipRect.left, excludeRect.top, excludeRect.left - 1, excludeRect.bottom);
		if (SPLIT_TEST(clipRect, quad4)) {
			quad4.Intersect(clipRect);
			AddRect(quad4);
		}

		Rect quad5(excludeRect.right + 1, excludeRect.top, clipRect.right, excludeRect.bottom);
		if (SPLIT_TEST(clipRect, quad5)) {
			quad5.Intersect(clipRect);
			AddRect(quad5);
		}

		Rect quad6(clipRect.left, excludeRect.bottom + 1, excludeRect.left - 1, clipRect.bottom);
		if (SPLIT_TEST(clipRect, quad6)) {
			quad6.Intersect(clipRect);
			AddRect(quad6);
		}

		Rect quad7(excludeRect.left, excludeRect.bottom + 1, excludeRect.right, clipRect.bottom);
		if (SPLIT_TEST(clipRect, quad7)) {
			quad7.Intersect(clipRect);
			AddRect(quad7);
		}

		Rect quad8(excludeRect.right + 1, excludeRect.bottom + 1, clipRect.right, clipRect.bottom);
		if (SPLIT_TEST(clipRect, quad8)) {
			quad8.Intersect(clipRect);
			AddRect(quad8);
		}

		// This rect has been split, remove it.  Note we don't
		// change the index
		RemoveRect(index);
		rectsToCheck--;
	}
	EndOperation();
	return *this;
}

Region& Region::Exclude(const Region &ex)
{
	BeginOperation();
	Region temp(ex);
	temp.Invert();
	Intersect(temp);
	EndOperation();
	return *this;
}



Region& Region::Clear()
{
	fNumRects = 0;
	AllocSpace(0);
	return *this;
}

Region& Region::ConstrainTo(const Rect &rect)
{
	BeginOperation();
	for (int i = fNumRects - 1; i >= 0; i--) {
		fRects[i].left = max(rect.left, fRects[i].left);
		fRects[i].right = min(rect.right, fRects[i].right);
		fRects[i].top = max(rect.top, fRects[i].top);
		fRects[i].bottom = min(rect.bottom, fRects[i].bottom);
		if (!fRects[i].Valid())
			RemoveRect(i);
	}

	EndOperation();
	return *this;
}

Region& Region::Translate(int x, int y)
{
	for (int i = 0; i < fNumRects; i++) {
		fRects[i].left += x;
		fRects[i].right += x;
		fRects[i].top += y;
		fRects[i].bottom += y;
	}

	return *this;
}

Region& Region::Intersect(const Region &intersectRegion)
{
	BeginOperation();
	Region newRegion;
	for (int i = 0; i < fNumRects; i++) {
		for (int j = 0; j < intersectRegion.fNumRects; j++) {
			if (RectAt(i).Intersects(intersectRegion.RectAt(j))) {
				Rect temp(RectAt(i));
				temp.Intersect(intersectRegion.RectAt(j));
				newRegion.AddRect(temp);
			}
		}
	}

	SetTo(newRegion);
	EndOperation();
	return *this;
}

bool Region::Intersects(const Rect &intersectsRect) const
{
	for (int i = 0; i < fNumRects; i++) {
		if(RectAt(i).Intersects(intersectsRect))
			return true;
	}
	return false;
}

Region& Region::Invert()
{
	BeginOperation();
	Region temp;
	temp.Include(Rect(-COORD_MAX, -COORD_MAX, COORD_MAX, COORD_MAX));
	for (int i = 0; i < fNumRects; i++)
		temp.Exclude(fRects[i]);

	SetTo(temp);
	EndOperation();
	return *this;
}

const bool Region::FindRect(int x, int y, Rect &outRect) const
{
	for (int i = 0; i < CountRects(); i++) {
		if (RectAt(i).Contains(x, y)) {
			outRect = RectAt(i);
			return true;
		}
	}

	return false;
}

void Region::AddRect(const Rect &rect)
{
	AllocSpace(fNumRects + 1);
	fRects[fNumRects++] = rect;
}

void Region::RemoveRect(int index)
{
	fNumRects--;
	memcpy(fRects + index, fRects + index + 1, (fNumRects - index) * sizeof(Rect));
	AllocSpace(fNumRects);
}

void Region::AddRegionRects(const Region &region)
{
	for (int i = 0; i < region.fNumRects; i++)
		AddRect(region.RectAt(i));
}

void Region::Consolidate()
{
	// Optimize region by consolidating adjacent rectangles.
	for (int i = 0; i < fNumRects; i++) {
		for (int j = fNumRects - 1; j > i; j--) {
			Rect &rect1 = fRects[i];
			Rect &rect2 = fRects[j];
			if (rect1.top == rect2.top && rect1.bottom == rect2.bottom) {
				if (rect1.right + 1 == rect2.left) {
					rect1.right = rect2.right;
					RemoveRect(j);
				} else if (rect2.right + 1 == rect1.left) {
					rect1.left = rect2.left;
					RemoveRect(j);
				}
			} else if (rect1.left == rect2.left && rect1.right == rect2.right) {
				if (rect1.top == rect2.bottom + 1) {
					rect1.top = rect2.top;
					RemoveRect(j);
				} else if (rect1.bottom + 1 == rect2.top) {
					rect1.bottom = rect2.bottom;
					RemoveRect(j);
				}
			}
		}
	}
}

void Region::AllocSpace(int numRects)
{
	int currentAlloc = ((fNumRects + kRectAllocSize - 1) / kRectAllocSize)
		* kRectAllocSize;
	int sizeWanted = ((numRects + kRectAllocSize - 1) / kRectAllocSize)
		* kRectAllocSize;

	if (sizeWanted != currentAlloc) {
		if (fRects == 0 && sizeWanted > 0)
			fRects = (Rect*) malloc(sizeWanted * sizeof(Rect));
		else if (sizeWanted == 0 && fRects != 0) {
			free(fRects);
			fRects = 0;
		} else
			fRects = (Rect*) realloc(fRects, sizeWanted * sizeof(Rect));
	}
}


void Region::BeginOperation()
{
	fOpLevel++;
}

void Region::EndOperation()
{
	if (--fOpLevel == 0)
		Consolidate();

	ASSERT(fOpLevel >= 0);
}



void Region::Dump() const
{
	printf("Region:  ");
	for (int i = 0; i < fNumRects; i++) {
		if ((i % 6) == 5)
			printf("\n");
		printf("(%d %d %d %d)   ", fRects[i].left, fRects[i].top, fRects[i].right,
			fRects[i].bottom);
	}

	printf("\n");
}
