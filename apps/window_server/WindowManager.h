#ifndef _WINDOW_MANAGER_H
#define _WINDOW_MANAGER_H

#include <sys/syscalls.h>
#include <win/Rect.h>

class Renderer;
class Window;

const int kMaxWindows = 255;

class WindowManager {
public:

	WindowManager(Renderer *screenRenderer);
	~WindowManager();

	int WaitForExit();

private:

	Window* CreateWindow(Window *parent, const Rect &rect, int eventPort, window_flags flags);
	void DestroyWindow(Window *window);
	Window* LookupWindow(int id);
	Window* WindowAtPoint(int x, int y);

	int RequestorPort() const;
	static int StartDispatchThread(void *windowManager);
	void DispatchThread();
	void ReadServicePort(void*, int);
	int ReadInt32();
	short ReadInt16();
	char ReadInt8();
	void Respond(port_id, void *, int);

	static int StartInputThread(void *_wm);
	void InputThread();

	void ProcessMouseEvent(const Event &event);
	void SetCursorPos(int x, int y);
	void LockCursor();
	void UnlockCursor();

	void InvalidateMouseBoundries();

	void ProcessKeyboardEvent(const Event &event);

	Window *fWindowArray[kMaxWindows];
	int fNextWindowID;

	port_id fServicePort;
	char *fReceiveBuffer;
	int fReceiveBufferSize;
	int fReceiveBufferPos;
	sem_id fCursorLock;

	Rect fMouseBoundries;
	Window *fCurrentMouseFocus;
	bool fMouseFocusLocked;
	bool fInFocusLockedWindow;

	Renderer *fScreenRenderer;
};


#endif
