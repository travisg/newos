#ifndef _RENDERER_VGA_H
#define _RENDERER_VGA_H

#include "Renderer_8bpp.h"

class Renderer_vga : public Renderer_8bpp {
public:
	Renderer_vga(char *baseAddress, int width, int height, int bytesPerRow);
	void DrawLine(int x1, int y1, int x2, int y2, char color);
	void FillRect(int x1, int y1, int x2, int y2, char color);
	void Blit(int x, int y, char image[], int image_width,
		int image_height, int img_bytes_per_row);
	void StretchBlit(const Rect &imageRect, const Rect &displayRect, char image[],
		int imageBytesPerRow);
	void CopyRect(const Rect &source, const Rect &dest);

	void SetCursorPosition(int x, int y);

private:
	void EraseCursor();
	void DrawCursor();

	char fSavedBackground[256];
	int fCursorX, fCursorY;
};


#endif
