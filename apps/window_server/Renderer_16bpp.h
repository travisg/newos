#ifndef _RENDERER_16BPP_H
#define _RENDERER_16BPP_H

#include "Renderer.h"

/* 16 bit per pixel, mostly colorspace neutral renderer */
class Renderer_16bpp : public Renderer {
public:
	Renderer_16bpp(char *baseAddress, int width, int height, int bytesPerRow, color_space cspace);

	virtual void DrawLine(int x1, int y1, int x2, int y2, color32 color, color_space cspace);
	virtual void FillRect(int x1, int y1, int x2, int y2, color32 color, color_space cspace);
	virtual void FillRect(const Rect &rect, color32 color, color_space cspace);
	virtual void CopyRect(const Rect &source, const Rect &dest);

	/* these must be implemented by a subclass, to deal with the color space conversions */
	virtual void Blit(int x, int y, const void *image, color_space cspace, int image_width,
		int image_height, int imageStrideWidth) = 0;
	virtual void StretchBlit(const Rect &imageRect, const Rect &displayRect, const void *image, color_space cspace,
		int imageStrideWidth) = 0;

	virtual void SetCursorPosition(int x, int y) {};

protected:
	color16 fTransparentColor;
};

class Renderer_RGB555 : public Renderer_16bpp {
public:
	inline Renderer_RGB555(char *baseAddress, int width, int height, int bytesPerRow);

	virtual void Blit(int x, int y, const void *image, color_space cspace, int image_width,
		int image_height, int imageStrideWidth);
	virtual void StretchBlit(const Rect &imageRect, const Rect &displayRect, const void *image, color_space cspace,
		int imageStrideWidth);
};

Renderer_RGB555::Renderer_RGB555(char *baseAddress, int width, int height, int bytesPerRow)
	:	Renderer_16bpp(baseAddress, width, height, bytesPerRow, CSPACE_RGB555)
{
}


#endif
