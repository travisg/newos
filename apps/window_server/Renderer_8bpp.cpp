#include "Renderer_8bpp.h"
#include "assert.h"

Renderer_8bpp::Renderer_8bpp(char *baseAddress, int width, int height, int bytesPerRow)
	:	Renderer(baseAddress, width, height, bytesPerRow)
{
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

void Renderer_8bpp::DrawLine(int x1, int y1, int x2, int y2, char color)
{
	ASSERT(x1 >= 0 && x1 < BufferWidth());
	ASSERT(x2 >= 0 && x2 < BufferWidth());
	ASSERT(y1 >= 0 && y1 < BufferWidth());
	ASSERT(y2 >= 0 && y2 < BufferWidth());

	// Swap if necessary so we always draw top to bottom 
	if (y1 > y2) {
		int temp = y1;
		y1 = y2;
		y2 = temp;
		
		temp = x1;
		x1 = x2;
		x2 = temp;
	}

	int deltaY = y2 - y1;
	int deltaX = x2 > x1 ? x2 - x1 : x1 - x2;
	int xDir = x2 > x1 ? 1 : -1;
	int error = 0;
	char *ptr = BufferBaseAddress() + x1 + y1 * BufferBytesPerRow();

	if (deltaX == 0) {
		// Vertical line
		for (int y = deltaY; y >= 0; y--) {
			*ptr = color;
			ptr += BufferBytesPerRow();
		}
	} else if (deltaY == 0) {
		// Horizontal line
		for (int x = deltaX; x >= 0; x--) {
			*ptr = color;
			ptr += xDir;
		}
	} else if (deltaX > deltaY) {
		// Diagonal with horizontal major axis
		x2 += xDir;	// Want to quit on pixel past last.  Ugly, I know.
		for (int x = x1; x != x2; x += xDir) {
			*ptr = color;

			error += deltaY;
			if (error > deltaX) {
				ptr += BufferBytesPerRow();
				error -= deltaX;	
			}

			ptr += xDir; 	
		}
	} else {
		// Diagonal with vertical major axis
		for (int y = y1; y <= y2; y++) {
			*ptr = color;

			error += deltaX;
			if (error > deltaY) {
				ptr += xDir;
				error -= deltaY;	
			}

			ptr += BufferBytesPerRow();
		}
	}
}

void Renderer_8bpp::FillRect(int x1, int y1, int x2, int y2, char color)
{
	ASSERT(x1 >= 0 && x1 <= BufferWidth());
	ASSERT(x2 >= 0 && x2 <= BufferWidth());
	ASSERT(y1 >= 0 && y1 <= BufferWidth());
	ASSERT(y2 >= 0 && y2 <= BufferWidth());
	ASSERT(x2 >= x1);
	ASSERT(y2 >= y1);

	char *ptr = BufferBaseAddress() + x1 + y1 * BufferBytesPerRow();
	unsigned int longcolor = (unsigned int)(unsigned char)color | ((unsigned int)(unsigned char)color << 8) |
		((unsigned int)(unsigned char)color << 16) | ((unsigned int)(unsigned char)color << 24);

	int wrap = BufferBytesPerRow() - (x2 - x1) - 1;
	for (int y = y1; y <= y2; y++) {
		int x = x1;
		while (x <= x2 && (x & 3) != 0) {
			*ptr++ = color;
			x++;
		}

		while (x + 4 <= x2) {
			*((long*) ptr) = longcolor;
			ptr += 4;
			x += 4;
		}
			
		while (x <= x2) {
			*ptr++ = color;				
			x++;
		}

		ptr += wrap;
	}
}

void Renderer_8bpp::Blit(int x, int y, char image[], int imageWidth,
    int imageHeight, int imageBytesPerRow)
{
	ASSERT(x >= 0 && x + imageWidth <= BufferWidth());
	ASSERT(y >= 0 && y + imageHeight <= BufferWidth());

	char *screen_ptr = BufferBaseAddress() + x + y * BufferBytesPerRow();
	char *image_ptr = image;
	int imageOffs = imageBytesPerRow - imageWidth;
	int screenOffs = BufferBytesPerRow() - imageWidth;

	for (int i = 0; i < imageHeight; i++) {
		for (int j = 0; j < imageWidth; j++) {
			if ((unsigned char) *image_ptr != 0xff)
				*screen_ptr = *image_ptr;

			screen_ptr++;
			image_ptr++;
		}

		image_ptr += imageOffs;
		screen_ptr += screenOffs;
	}
}

void Renderer_8bpp::StretchBlit(const Rect &imageRect, const Rect &displayRect, char image[],
	int imageBytesPerRow)
{
	ASSERT(imageRect.Valid());
	ASSERT(displayRect.Valid());
	ASSERT(imageRect.left >= 0);
	ASSERT(imageRect.top >= 0);
	ASSERT(imageRect.right < imageBytesPerRow);
	
	int verror = 0;
	char *screen_ptr = BufferBaseAddress() + displayRect.left + displayRect.top
		* BufferBytesPerRow();
	char *current_image_line = image + imageRect.left + imageRect.top *
		imageBytesPerRow;
	int screenOffs = BufferBytesPerRow() - displayRect.Width() - 1;
	
	for (int y = displayRect.top; y <= displayRect.bottom; y++) {
		char *image_ptr = current_image_line;
		int herror = 0;
		for (int x = displayRect.left; x <= displayRect.right; x++) {
			if ((unsigned char) *image_ptr != 0xff)
				*screen_ptr = *image_ptr;

			herror += imageRect.Width();
			while (herror >= displayRect.Width()) {
				herror -= displayRect.Width();
 				image_ptr++;
			}			
	
			screen_ptr++;
		}

		verror += imageRect.Height();
		while (verror > displayRect.Height()) {
			verror -= displayRect.Height();
			current_image_line += imageBytesPerRow;
		}
		
		screen_ptr += screenOffs;
	}
}

void Renderer_8bpp::CopyRect(const Rect &source, const Rect &dest)
{
	ASSERT(source.Width() == dest.Width());
	ASSERT(source.Height() == dest.Height());

	int dX;
	int dY;	
	int startX;
	int startY;	
	if (dest.left > source.left && dest.left < source.right) {
		// overlap left side, copy backward
		dX = -1;
		startX = source.right;
	} else {
		dX = 1;
		startX = source.left;
	}

	if (dest.top > source.top && dest.top < source.bottom) {
		// overlap bottom, copy top up
		dY = -(BufferBytesPerRow() + dX * (source.Width() + 1));
		startY = source.bottom;
	} else {
		dY = (BufferBytesPerRow() - dX * (source.Width() - 1));
		startY = source.top;
	}
	
	char *srcptr = BufferBaseAddress() + startX + startY * BufferBytesPerRow();
	int offset =  (dest.left - source.left) + (dest.top - source.top)
		* BufferBytesPerRow();
	
	for (int y = source.top; y <= source.bottom; y++) {
		for (int x = source.left; x <= source.right; x++) {
			*((char*) srcptr + offset) = *srcptr;
			srcptr += dX;
		}

		srcptr += dY;
	}
}



