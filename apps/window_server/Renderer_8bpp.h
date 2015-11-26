#ifndef _RENDERER_8BPP_H
#define _RENDERER_8BPP_H

#include "Renderer.h"

class Renderer_8bpp : public Renderer {
public:
    Renderer_8bpp(char *baseAddress, int width, int height, int bytesPerRow);
    void DrawLine(int x1, int y1, int x2, int y2, char color);
    void FillRect(int x1, int y1, int x2, int y2, char color);
    void Blit(int x, int y, char image[], int image_width,
              int image_height, int img_bytes_per_row);
    void StretchBlit(const Rect &imageRect, const Rect &displayRect, char image[],
                     int imageBytesPerRow);
    void CopyRect(const Rect &source, const Rect &dest);
};


#endif
