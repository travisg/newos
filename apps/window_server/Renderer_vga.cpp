#include "Renderer_vga.h"
#include "assert.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

const unsigned char kCursorBits [] = {
	0x3f,0x3f,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0x3f,0x3f,0x3f,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0x3f,0x3f,0x3f,0x3f,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0x3f,0x3f,0x3f,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0x3f,0x3f,0x3f,0xff,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0x3f,0x00,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0x00,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0x00,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0x00,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0x00,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

Renderer_vga::Renderer_vga(char *baseAddress, int width, int height, int bytesPerRow)
	:	Renderer_8bpp(baseAddress, width, height, bytesPerRow)
{
	fCursorX = width / 2;
	fCursorY = height / 2;
	DrawCursor();
}

const unsigned kBottom = 1;
const unsigned kTop = 2;
const unsigned kLeft = 4;
const unsigned kRight = 8;

#define clipmask(x, y, rect) 		\
	({	unsigned mask = 0;								\
		if (x < rect.left) mask |= kLeft;			\
		else if (x > rect.right) mask |= kRight;	\
		if (y < rect.top) mask |= kTop;			\
		else if (y > rect.bottom) mask |= kBottom;	\
		mask;})											

inline int vert_intersection(int x1, int y1, int x2, int y2, int x)
{
	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

inline int horz_intersection(int x1, int y1, int x2, int y2, int y)
{
	return x1 + (x2 - x1) * (y - y1) / (y2 - y1);
}

void Renderer_vga::DrawLine(int x1, int y1, int x2, int y2, char color)
{
	Rect cursorRect(fCursorX, fCursorY, fCursorX + 16, fCursorY + 16);
	bool invalidateCursor = true;

	int clippedX1 = x1;
	int clippedY1 = y1;
	int clippedX2 = x2;
	int clippedY2 = y2;
	unsigned point1mask = clipmask(clippedX1, clippedY1, cursorRect);
	unsigned point2mask = clipmask(clippedX2, clippedY2, cursorRect);
	while (point1mask != 0 || point2mask != 0) {
		if ((point1mask & point2mask) != 0) {
			invalidateCursor = false;
			break;
		}

		unsigned  mask = point1mask ? point1mask : point2mask;
		int x = 0;
		int y = 0;
		if (mask & kBottom) {
			y = cursorRect.bottom;
			x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
		} else if (mask & kTop) {
			y = cursorRect.top;
			x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
		} else if (mask & kRight) {
			x = cursorRect.right;
			y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
		} else if (mask & kLeft) {
			x = cursorRect.left;
			y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
		}
		
		if (point1mask) {
			// Clip point 1
			point1mask = clipmask(x, y, cursorRect);
			clippedX1 = x;
			clippedY1 = y;
		} else {
			// Clip point 2
			point2mask = clipmask(x, y, cursorRect);
			clippedX2 = x;
			clippedY2 = y;
		}
	}

	if (invalidateCursor)
		EraseCursor();
	
	Renderer_8bpp::DrawLine(x1, y1, x2, y2, color);
	
	if (invalidateCursor)
		DrawCursor();
}

void Renderer_vga::FillRect(int x1, int y1, int x2, int y2, char color)
{

	bool invalidateCursor = false;
	if (Rect(x1, y1, x2, y2).Intersects(Rect(fCursorX, fCursorY, fCursorX + 16,
		fCursorY + 16))) {
		invalidateCursor = true;
		EraseCursor();
	}

	Renderer_8bpp::FillRect(x1, y1, x2, y2, color);
	if (invalidateCursor)
		DrawCursor();
}

void Renderer_vga::Blit(int x, int y, char image[], int imageWidth,
    int imageHeight, int imageBytesPerRow)
{

	bool invalidateCursor = false;
	if (Rect(x, y, x + imageWidth, y + imageHeight).Intersects(Rect(fCursorX,
		fCursorY, fCursorX + 16, fCursorY + 16))) {
		invalidateCursor = true;
		EraseCursor();
	}

	Renderer_8bpp::Blit(x, y, image, imageWidth, imageHeight, imageBytesPerRow);
	if (invalidateCursor)
		DrawCursor();
}

void Renderer_vga::StretchBlit(const Rect &imageRect, const Rect &displayRect, char image[],
	int imageBytesPerRow)
{

	bool invalidateCursor = false;
	if (displayRect.Intersects(Rect(fCursorX, fCursorY, fCursorX + 16,
		fCursorY + 16))) {
		invalidateCursor = true;
		EraseCursor();
	}

	Renderer_8bpp::StretchBlit(imageRect, displayRect, image, imageBytesPerRow);

	if (invalidateCursor)
		DrawCursor();
}

void Renderer_vga::CopyRect(const Rect &source, const Rect &dest)
{
	bool invalidateCursor = false;
	Rect cursorRect(fCursorX, fCursorY, fCursorX + 16, fCursorY + 16);
	if (cursorRect.Intersects(source) || cursorRect.Intersects(dest)) {
		invalidateCursor = true;
		EraseCursor();
	}

	Renderer_8bpp::CopyRect(source, dest);
	if (invalidateCursor)
		DrawCursor(); 
}

void Renderer_vga::SetCursorPosition(int x, int  y)
{
	EraseCursor();
	fCursorX = x;
	fCursorY = y;
	DrawCursor();
}

void Renderer_vga::EraseCursor()
{
	char *screen_ptr = BufferBaseAddress() + fCursorX + fCursorY * BufferBytesPerRow();
	char *saveBackground = fSavedBackground;
	int cursorDisplayWidth = MIN(16, BufferBytesPerRow() - fCursorX);
	int cursorDisplayHeight = MIN(16, BufferHeight() - fCursorY);
	int stride = BufferBytesPerRow() - cursorDisplayWidth;

	for (int y = 0; y < cursorDisplayHeight; y++) {
		for (int x = 0; x < cursorDisplayWidth; x++) {
			if ((unsigned char) *saveBackground != 0xff)
				*screen_ptr = *saveBackground;

			screen_ptr++;
			saveBackground++;
		}

		screen_ptr += stride;
		saveBackground += 16 - cursorDisplayWidth;
	}
}

void Renderer_vga::DrawCursor()
{
	char *screen_ptr = BufferBaseAddress() + fCursorX + fCursorY * BufferBytesPerRow();
	char *cursorImage = (char *) kCursorBits;
	char *saveBackground = fSavedBackground;
	int cursorDisplayWidth = MIN(16, BufferBytesPerRow() - fCursorX);
	int cursorDisplayHeight = MIN(16, BufferHeight() - fCursorY);
	int stride = BufferBytesPerRow() - cursorDisplayWidth;

	for (int y = 0; y < cursorDisplayHeight; y++) {
		for (int x = 0; x < cursorDisplayWidth; x++) {
			if ((unsigned char) *cursorImage != 0xff) {
				*saveBackground = *screen_ptr;
				*screen_ptr = *cursorImage;
			} else
				*saveBackground = 0xff;

			screen_ptr++;
			cursorImage++;
			saveBackground++;
		}

		screen_ptr += stride;
		cursorImage += 16 - cursorDisplayWidth;
		saveBackground += 16 - cursorDisplayWidth;
	}
}
