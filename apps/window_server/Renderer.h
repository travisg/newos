#ifndef _RENDERER_H
#define _RENDERER_H

#include <sys/types.h>
#include <win/Color.h>
#include <win/Rect.h>

using namespace os::gui;

class Renderer {
public:

    inline Renderer(char *baseAddress, int width, int height, int bytesPerRow, color_space cspace);
    virtual void DrawLine(int x1, int y1, int x2, int y2, color32 color, color_space cspace) = 0;
    virtual void FillRect(int x1, int y1, int x2, int y2, color32 color, color_space cspace) = 0;
    virtual void FillRect(const Rect &rect, color32 color, color_space cspace) = 0;
    virtual void Blit(int x, int y, const void *image, color_space cspace, int image_width,
                      int image_height, int imageStrideWidth) = 0;
    virtual void StretchBlit(const Rect &imageRect, const Rect &displayRect, const void *image, color_space cspace,
                             int imageStrideWidth) = 0;
    virtual void CopyRect(const Rect &source, const Rect &dest) = 0;

    virtual void SetCursorPosition(int x, int y) = 0;

    inline Rect Bounds() const;

    inline char *BufferBaseAddress() const;
    inline int BufferWidth() const;
    inline int BufferHeight() const;
    inline int BufferBytesPerRow() const;
    inline color_space GetColorspace() const;

private:
    char *fBufferBaseAddress;
    int fBufferWidth;
    int fBufferHeight;
    int fBufferBytesPerRow;
    color_space fColorSpace;
};

inline Renderer::Renderer(char *baseAddress, int width, int height, int bytesPerRow, color_space cspace)
    :   fBufferBaseAddress(baseAddress),
        fBufferWidth(width),
        fBufferHeight(height),
        fBufferBytesPerRow(bytesPerRow),
        fColorSpace(cspace)
{
}

inline char* Renderer::BufferBaseAddress() const
{
    return fBufferBaseAddress;
}

inline int Renderer::BufferWidth() const
{
    return fBufferWidth;
}

inline int Renderer::BufferHeight() const
{
    return fBufferHeight;
}

inline int Renderer::BufferBytesPerRow() const
{
    return fBufferBytesPerRow;
}

inline Rect Renderer::Bounds() const
{
    return Rect(0, 0, fBufferWidth - 1, fBufferHeight - 1);
}

inline color_space Renderer::GetColorspace() const
{
    return fColorSpace;
}

#endif

