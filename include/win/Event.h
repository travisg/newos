#ifndef _EVENT_H
#define _EVENT_H

namespace os {
namespace gui {

enum {
	EVT_PAINT = 0,
	EVT_QUIT,
	EVT_MOUSE_DOWN,
	EVT_MOUSE_UP,
	EVT_MOUSE_MOVED,
	EVT_MOUSE_ENTER,
	EVT_MOUSE_LEAVE,
	EVT_KEY_DOWN,
	EVT_KEY_UP
};

struct Event {
	int what;
	int target;

	int key;
	int modifiers;
	int x, y;
};

enum {
	EVT_MOD_KEYDOWN = 0x1,
	EVT_MOD_KEYUP = 0x2,
	EVT_MOD_SHIFT = 0x4,
	EVT_MOD_CTRL = 0x8,
	EVT_MOD_ALT = 0x10,
	EVT_MOD_WIN = 0x20,
	EVT_MOD_MENU = 0x40,
};

}
}

#endif

