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

}
}

#endif

