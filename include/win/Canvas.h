#ifndef _WIN_CANVAS_H
#define _WIN_CANVAS_H

#include <win/Color.h>
#include <win/Rect.h>
#include <win/WindowFlags.h>

namespace os {
namespace gui {

class Window;
class Event;
class Rect;

class Canvas {
public:

	Canvas();
	virtual ~Canvas();

	void AddChild(Canvas *child, const Rect &childRect, window_flags flags = WINDOW_FLAG_NONE);
	void RemoveChild(Canvas *child);
	void DrawLine(int x1, int y1, int x2, int y2);
	void FillRect(const Rect &r);
	void SetBackgroundColor(color888);
	void SetPenColor(color888);
	void DrawString(int x, int y, const char*);
	void Hide();
	void Show();
	void CopyRect(const Rect &r, int newLeft, int newTop);
	void CopyRect(const Rect &r, const Rect &r2);
	Window* GetWindow() const;
	Rect GetBounds() const;
	void Invalidate();
	void Invalidate(const Rect &r);
	void LockMouseFocus();

	virtual void Repaint(const Rect &dirtyRect);
	virtual void EventReceived(const Event*);

private:

	void HandleEvent(Event*);
	void BeginPaint(Rect &r);
	void EndPaint();

	int fID;
	Window *fWindow;
	bool fInPaint;
	int fShowLevel;
	Canvas *fWinListNext, **fWinListPrev;
	Rect fBounds;

	friend class Window;
};

}
}
#endif

