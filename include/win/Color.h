#ifndef _WIN_COLOR_H
#define _WIN_COLOR_H

#include <sys/types.h>
#include <stdio.h>

namespace os {
namespace gui {

// some color conversion routines

typedef uint16 color565;
typedef uint16 color555;
typedef uint32 color888;

typedef uint16 color16;
typedef uint32 color32;

typedef enum {
	CSPACE_UNKNOWN = 0,
	CSPACE_8BIT,
	CSPACE_RGB555,
	CSPACE_RGB565,
	CSPACE_RGB888,
	CSPACE_RGB8888,
} color_space;

// some nasty pinkish color
const color555 TRANS555COLOR = 0x68ab;
const color565 TRANS565COLOR = 0xd16b;
const color888 TRANS888COLOR = 0x00d52f5c;

inline color565 Color888to565(color888 incolor)
{
	color565 r = (incolor >> 19) & 0x1f;
	color565 g = (incolor >> 10) & 0x3f;
	color565 b = (incolor >> 3) & 0x1f;

	return (r << 11) | (g << 5) | b;
}

inline color888 Color565to888(color565 incolor)
{
	color888 r = (incolor >> 11) & 0x1f;
	color888 g = (incolor >> 5) & 0x3f;
	color888 b = incolor & 0x1f;

	return (r << 19) | (g << 10) | (b << 3);
}

inline color555 Color888to555(color888 incolor)
{
	color555 r = (incolor >> 19) & 0x1f;
	color555 g = (incolor >> 11) & 0x1f;
	color555 b = (incolor >> 3) & 0x1f;

	return (r << 10) | (g << 5) | b;
}

inline color888 Color555to888(color555 incolor)
{
	color888 r = (incolor >> 10) & 0x1f;
	color888 g = (incolor >> 5) & 0x1f;
	color888 b = incolor & 0x1f;

	return (r << 19) | (g << 11) | (b << 3);
}

inline color555 Color565to555(color565 incolor)
{
	return (incolor & 0x1f) | ((incolor & 0xffe0) >> 1);
}

inline color565 Color555to565(color555 incolor)
{
	return (incolor & 0x1f) | ((incolor & 0x7fe0) << 1);
}

inline color32 ColorTranslate(color32 incolor, color_space incspace, color_space outcspace)
{
	if(incspace == outcspace)
		return incolor;

	switch(outcspace) {
		case CSPACE_RGB555:
			switch(incspace) {
				case CSPACE_RGB565:
					return Color565to555(incolor);
				case CSPACE_RGB888:
					return Color888to555(incolor);
			}
			break;
		case CSPACE_RGB565:
			switch(incspace) {
				case CSPACE_RGB555:
					return Color555to565(incolor);
				case CSPACE_RGB888:
					return Color888to565(incolor);
			}
			break;
		case CSPACE_RGB888:
			switch(incspace) {
				case CSPACE_RGB555:
					return Color888to555(incolor);
				case CSPACE_RGB565:
					return Color888to565(incolor);
			}
			break;
	}

	printf("ColorTranslate: unhandled color_space (incspace %d, outcspace %d\n", incspace, outcspace);
	return 0;
}

}
}
#endif

