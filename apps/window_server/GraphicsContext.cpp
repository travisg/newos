#include <win/Rect.h>
#include "Renderer.h"
#include "Region.h"
#include "GraphicsContext.h"
#include "util.h"
#include "font.h"
#include "Window.h"

#define swap(a, b) { int temp = a; a = b; b = temp; }


GraphicsContext::GraphicsContext()
	:	fRenderer(0)
{
}

const unsigned kBottom = 1;
const unsigned kTop = 2;
const unsigned kLeft = 4;
const unsigned kRight = 8;

inline unsigned clipmask(int x, int y, const Rect &rect)
{
	unsigned mask = 0;
	if (x < rect.left)
		mask |= kLeft;
	else if (x > rect.right)
		mask |= kRight;

	if (y < rect.top)
		mask |= kTop;
	else if (y > rect.bottom)
		mask |= kBottom;

	return mask;
}

inline int vert_intersection(int x1, int y1, int x2, int y2, int x)
{
	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

inline int horz_intersection(int x1, int y1, int x2, int y2, int y)
{
	return x1 + (x2 - x1) * (y - y1) / (y2 - y1);
}

void GraphicsContext::DrawLine(int x1, int y1, int x2, int y2)
{
	x1 += fXOrigin;
	x2 += fXOrigin;
	y1 += fYOrigin;
	y2 += fYOrigin;

	for (int rect = 0; rect < fClipRegion.CountRects(); rect++) {
		const Rect &clipRect = fClipRegion.RectAt(rect);
		int clippedX1 = x1;
		int clippedY1 = y1;
		int clippedX2 = x2;
		int clippedY2 = y2;
		unsigned point1mask = clipmask(clippedX1, clippedY1, clipRect);
		unsigned point2mask = clipmask(clippedX2, clippedY2, clipRect);

		bool rejected = false;
		while (point1mask != 0 || point2mask != 0) {
			if ((point1mask & point2mask) != 0) {
				rejected = true;
				break;
			}

			unsigned  mask = point1mask ? point1mask : point2mask;
			int x = 0;
			int y = 0;
			if (mask & kBottom) {
				y = clipRect.bottom;
				x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
			} else if (mask & kTop) {
				y = clipRect.top;
				x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
			} else if (mask & kRight) {
				x = clipRect.right;
				y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
			} else if (mask & kLeft) {
				x = clipRect.left;
				y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
			}

			if (point1mask) {
				// Clip point 1
				point1mask = clipmask(x, y, clipRect);
				clippedX1 = x;
				clippedY1 = y;
			} else {
				// Clip point 2
				point2mask = clipmask(x, y, clipRect);
				clippedX2 = x;
				clippedY2 = y;
			}
		}

		if (!rejected)
	 		fRenderer->DrawLine(clippedX1, clippedY1, clippedX2, clippedY2, fCurrentColor, CSPACE_RGB888);
	}
}

void GraphicsContext::FillRect(int x1, int y1, int x2, int y2)
{
	x1 += fXOrigin;
	x2 += fXOrigin;
	y1 += fYOrigin;
	y2 += fYOrigin;

    if (x1 > x2) swap(x1, x2);
    if (y1 > y2) swap(y1, y2);

	for (int rect = 0; rect < fClipRegion.CountRects(); rect++) {
		const Rect &clipRect = fClipRegion.RectAt(rect);
		int clipleft = max(x1, clipRect.left);
		int cliptop = max(y1, clipRect.top);
		int clipright = min(x2, clipRect.right);
		int clipbottom = min(y2, clipRect.bottom);

		if (clipright >= clipleft && clipbottom >= cliptop)
			fRenderer->FillRect(clipleft, cliptop, clipright, clipbottom,
				fCurrentColor, CSPACE_RGB888);
	}
}

void GraphicsContext::Blit(int x, int y, color888 image[], int image_width,
	int image_height, int imageStrideWidth)
{
	x += fXOrigin;
	y += fYOrigin;

	for (int rect = 0; rect < fClipRegion.CountRects(); rect++) {
		const Rect &clipRect = fClipRegion.RectAt(rect);
		int clipleft = max(x, clipRect.left);
		int cliptop = max(y, clipRect.top);
		int clipright = min(x + image_width - 1, clipRect.right);
		int clipbottom = min(y + image_height - 1, clipRect.bottom);

		if (clipright >= clipleft && clipbottom >= cliptop) {
			fRenderer->Blit(clipleft, cliptop, image + (clipleft - x) +
				(cliptop - y) * imageStrideWidth, CSPACE_RGB888, clipright - clipleft + 1,
				clipbottom - cliptop + 1, imageStrideWidth);
		}
	}
}

void GraphicsContext::StretchBlit(Rect imageRect, Rect screenRect, color888 image[], int imageStrideWidth)
{
	screenRect.left += fXOrigin;
	screenRect.top += fYOrigin;
	screenRect.right += fXOrigin;
	screenRect.bottom += fYOrigin;

	for (int rect = 0; rect < fClipRegion.CountRects(); rect++) {
		const Rect &clipRect = fClipRegion.RectAt(rect);
		Rect clippedScreenRect;
		clippedScreenRect.left = max(screenRect.left, clipRect.left);
		clippedScreenRect.top = max(screenRect.top, clipRect.top);
		clippedScreenRect.right = min(screenRect.right, clipRect.right);
		clippedScreenRect.bottom = min(screenRect.bottom, clipRect.bottom);

		if (clippedScreenRect.Width() > 0 && clippedScreenRect.Height() > 0) {
			Rect clippedImageRect;
			clippedImageRect.left = (clippedScreenRect.left - screenRect.left)
				* imageRect.Width() / screenRect.Width();
			clippedImageRect.right = (clippedScreenRect.right - screenRect.left)
				 * imageRect.Width() / screenRect.Width();
			clippedImageRect.top = (clippedScreenRect.top - screenRect.top)
				* imageRect.Height() / screenRect.Height();
			clippedImageRect.bottom = (clippedScreenRect.bottom - screenRect.top)
				* imageRect.Height() / screenRect.Height();

			fRenderer->StretchBlit(clippedImageRect, clippedScreenRect, image, CSPACE_RGB888, imageStrideWidth);
		}
	}
}

void GraphicsContext::CopyRect(Rect src, Rect dest, const Region &inCleanRegion,
	Region &outNotCopied)
{
	src.OffsetBy(fXOrigin, fYOrigin);
	dest.OffsetBy(fXOrigin, fYOrigin);

	int dX = dest.left - src.left;
	int dY = dest.top - src.top;
	Region copyRegion = inCleanRegion;
	copyRegion.ConstrainTo(dest);
	copyRegion.Translate(-dX, -dY);
	copyRegion.Intersect(inCleanRegion);
	for (int rect = 0; rect < copyRegion.CountRects(); rect++) {
		const Rect &srcRect = copyRegion.RectAt(rect);
		Rect destRect(srcRect);
		destRect.OffsetBy(dX, dY);
		fRenderer->CopyRect(srcRect, destRect);
	}

	copyRegion.Invert();
	copyRegion.Intersect(fClipRegion);
	outNotCopied = copyRegion;
}

void GraphicsContext::DrawString(int x, int y, const char *string)
{
	while (*string) {
		color888 glyph[5*8];
		const unsigned char *font_ptr = &font5x8[*string * (5*8)];

		for(int i = 0; i < 5*8; i++) {
			if(*font_ptr++ != 0)
				glyph[i] = fCurrentColor;
			else
				glyph[i] = TRANS888COLOR;
		}

		Blit(x, y, glyph, 5, 8, 5);
		x += 5;
		string++;
	}
}

Rect GraphicsContext::Bounds()
{
	Rect rect = fClipRegion.Bounds();
	rect.OffsetBy(-fXOrigin, -fYOrigin);
	return rect;
}



