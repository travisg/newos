#ifndef _RENDERER_VESA_H
#define _RENDERER_VESA_H

#include "Renderer_16bpp.h"

class Renderer_vesa_555 : public Renderer_RGB555 {
public:
    Renderer_vesa_555(char *baseAddress, int width, int height, int bytesPerRow);
    virtual void DrawLine(int x1, int y1, int x2, int y2, color32 color, color_space cspace);
    virtual void FillRect(int x1, int y1, int x2, int y2, color32 color, color_space cspace);
    virtual void CopyRect(const Rect &source, const Rect &dest);

    virtual void Blit(int x, int y, const void *image, color_space cspace, int image_width,
                      int image_height, int imageStrideWidth);
    virtual void StretchBlit(const Rect &imageRect, const Rect &displayRect, const void *image, color_space cspace,
                             int imageStrideWidth);

    virtual void SetCursorPosition(int x, int y);

private:
    void EraseCursor();
    void DrawCursor();

    color555 fSavedBackground[256];
    int fCursorX, fCursorY;
};

#endif
