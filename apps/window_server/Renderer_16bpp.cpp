#include "Renderer_16bpp.h"
//#include "assert.h"

#define ASSERT(x)

Renderer_16bpp::Renderer_16bpp(char *baseAddress, int width, int height, int bytesPerRow, color_space cspace)
    :   Renderer(baseAddress, width, height, bytesPerRow, cspace)
{
    switch (cspace) {
        case CSPACE_RGB555:
            fTransparentColor = TRANS555COLOR;
            break;
        case CSPACE_RGB565:
            fTransparentColor = TRANS565COLOR;
            break;
        default:
            // error
            fTransparentColor = 0;
    }
}

const unsigned kBottom = 1;
const unsigned kTop = 2;
const unsigned kLeft = 4;
const unsigned kRight = 8;

#define clipmask(x, y, rect)        \
    ({  unsigned int mask = 0;                              \
        if (x < rect.left) mask |= kLeft;           \
        else if (x > rect.right) mask |= kRight;    \
        if (y < rect.top) mask |= kTop;         \
        else if (y > rect.bottom) mask |= kBottom;  \
        mask;})

void Renderer_16bpp::CopyRect(const Rect &source, const Rect &dest)
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
        dY = -((BufferBytesPerRow() / 2) + dX * source.Width()) + 1;
        startY = source.bottom;
    } else {
        dY = ((BufferBytesPerRow() / 2) - dX * source.Width()) - 1;
        startY = source.top;
    }

    uint16 *srcptr = (uint16 *)BufferBaseAddress() + startX + startY * (BufferBytesPerRow() / 2);
    int offset =  (dest.left - source.left) + (dest.top - source.top)
                  * (BufferBytesPerRow() / 2);

    for (int y = source.top; y <= source.bottom; y++) {
        for (int x = source.left; x <= source.right; x++) {
            *((uint16 *) srcptr + offset) = *srcptr;
            srcptr += dX;
        }

        srcptr += dY;
    }
}

void Renderer_16bpp::FillRect(int x1, int y1, int x2, int y2, color32 _color, color_space cspace)
{
    ASSERT(x1 >= 0 && x1 <= BufferWidth());
    ASSERT(x2 >= 0 && x2 <= BufferWidth());
    ASSERT(y1 >= 0 && y1 <= BufferWidth());
    ASSERT(y2 >= 0 && y2 <= BufferWidth());
    ASSERT(x2 >= x1);
    ASSERT(y2 >= y1);

    color16 *ptr = (color16 *)BufferBaseAddress() + x1 + y1 * (BufferBytesPerRow() / 2);
    color16 color = ColorTranslate(_color, cspace, GetColorspace());
    uint32 longcolor = ((uint32)color & 0xffff) | ((uint32)color << 16);

    int wrap = BufferWidth() - (x2 - x1) - 1;
    for (int y = y1; y <= y2; y++) {
        int x = x1;
        while (x <= x2 && (x & 3) != 0) {
            *ptr++ = color;
            x++;
        }

        while (x + 2 <= x2) {
            *((uint32 *) ptr) = longcolor;
            ptr += 2;
            x += 2;
        }

        while (x <= x2) {
            *ptr++ = color;
            x++;
        }

        ptr += wrap;
    }
}

void Renderer_16bpp::FillRect(const Rect &rect, color32 color, color_space cspace)
{
    FillRect(rect.left, rect.top, rect.right, rect.bottom, color, cspace);
}

void Renderer_16bpp::DrawLine(int x1, int y1, int x2, int y2, color32 _color, color_space cspace)
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
    color16 *ptr = (color16 *)BufferBaseAddress() + x1 + y1 * (BufferBytesPerRow() / 2);
    color16 color = ColorTranslate(_color, cspace, GetColorspace());

    if (deltaX == 0) {
        // Vertical line
        for (int y = deltaY; y >= 0; y--) {
            *ptr = color;
            ptr += (BufferBytesPerRow() / 2);
        }
    } else if (deltaY == 0) {
        // Horizontal line
        for (int x = deltaX; x >= 0; x--) {
            *ptr = color;
            ptr += xDir;
        }
    } else if (deltaX > deltaY) {
        // Diagonal with horizontal major axis
        x2 += xDir; // Want to quit on pixel past last.  Ugly, I know.
        for (int x = x1; x != x2; x += xDir) {
            *ptr = color;

            error += deltaY;
            if (error > deltaX) {
                ptr += (BufferBytesPerRow() / 2);
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

            ptr += (BufferBytesPerRow() / 2);
        }
    }
}

void Renderer_RGB555::Blit(int x, int y, const void *image, color_space cspace, int imageWidth,
                           int imageHeight, int imageStrideWidth)
{
    ASSERT(x >= 0 && x + imageWidth <= BufferWidth());
    ASSERT(y >= 0 && y + imageHeight <= BufferWidth());

    uint16 *screen_ptr = (uint16 *)BufferBaseAddress() + x + y * (BufferBytesPerRow() / 2);
    color16 *image_ptr16 = (color16 *)image;
    color32 *image_ptr32 = (color32 *)image;
    int imageOffs = imageStrideWidth - imageWidth;
    int screenOffs = (BufferBytesPerRow() / 2) - imageWidth;

    if (cspace == CSPACE_RGB555) {
        for (int i = 0; i < imageHeight; i++) {
            for (int j = 0; j < imageWidth; j++) {
                if (*image_ptr16 != TRANS555COLOR)
                    *screen_ptr = *image_ptr16;
                screen_ptr++;
                image_ptr16++;
            }
            image_ptr16 += imageOffs;
            screen_ptr += screenOffs;
        }
    } else if (cspace == CSPACE_RGB888) {
        for (int i = 0; i < imageHeight; i++) {
            for (int j = 0; j < imageWidth; j++) {
                if (*image_ptr32 != TRANS888COLOR)
                    *screen_ptr = Color888to555(*image_ptr32);
                screen_ptr++;
                image_ptr32++;
            }
            image_ptr32 += imageOffs;
            screen_ptr += screenOffs;
        }
    } else if (cspace == CSPACE_RGB565) {
        for (int i = 0; i < imageHeight; i++) {
            for (int j = 0; j < imageWidth; j++) {
                if (*image_ptr16 != TRANS565COLOR)
                    *screen_ptr = Color565to555(*image_ptr16);
                screen_ptr++;
                image_ptr16++;
            }
            image_ptr16 += imageOffs;
            screen_ptr += screenOffs;
        }
    }
}

// XXX broken
void Renderer_RGB555::StretchBlit(const Rect &imageRect, const Rect &displayRect, const void *image, color_space cspace,
                                  int imageStrideWidth)
{
#if 0
    ASSERT(imageRect.Valid());
    ASSERT(displayRect.Valid());
    ASSERT(imageRect.left >= 0);
    ASSERT(imageRect.top >= 0);
    ASSERT(imageRect.right < imageStrideWidth);

    int verror = 0;
    char *screen_ptr = BufferBaseAddress() + displayRect.left + displayRect.top
                       * BufferBytesPerRow();
    color888 *current_image_line = image + imageRect.left + imageRect.top *
                                   imageStrideWidth;
    int screenOffs = BufferBytesPerRow() - displayRect.Width() - 1;

    for (int y = displayRect.top; y <= displayRect.bottom; y++) {
        color888 *image_ptr = current_image_line;
        int herror = 0;
        for (int x = displayRect.left; x <= displayRect.right; x++) {
            if (*image_ptr != TRANS888COLOR)
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
            current_image_line += imageStrideWidth;
        }

        screen_ptr += screenOffs;
    }
#endif
}


