#ifndef _WIN_WINDOW_H
#define _WIN_WINDOW_H

class Canvas;
class Connection;
class Rect;
struct Event;

class Window {
public:

	Window(const Rect &);
	void Quit();
	void AddChild(Canvas *child, const Rect &);
	void AddBorderChild(Canvas *child, const Rect &);
	void RemoveChild(Canvas *child);
	void Flush();
	void MakeFocus();
	void Lock();
	void Unlock();
	void Show();
	void Hide();
	Canvas *GetCanvas();

private:

	~Window();
	static int EventLoop(void*);
	Canvas* FindChild(int id);
	void DispatchEvent(Event*);
	void WaitEvent(Event*);

	Connection *fConnection;
	int fID;
	int fShowLevel;
	int fEventPort;
	sem_id fLock;

	Canvas *fCanvasList;
	Canvas *fBorderCanvas;
	Canvas *fBackgroundCanvas;

	friend class Canvas;
};


#endif

