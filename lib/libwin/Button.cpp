#include <win/Event.h>
#include <win/Button.h>
#include <win/Rect.h>
#include <stdlib.h>
#include <string.h>

namespace os {
namespace gui {

Button::Button(const char *text)
	:	fMouseDown(false),
		fOverButton(false)
{
	fText = (char*) malloc(strlen(text) + 1);
	memcpy(fText, text, strlen(text) + 1);
}

Button::~Button()
{
	free(fText);
}

void Button::Invoke()
{
}

void Button::Repaint(const Rect &dirtyRect)
{
	Rect bounds = GetBounds();

	if (fMouseDown && fOverButton) {
		bounds.left++;
		bounds.top++;
	} else {
		bounds.right--;
		bounds.bottom--;
	}

	SetPenColor(0x00c4c0b8); // grey
	FillRect(Rect(bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1));
	SetPenColor(0); // black
	DrawLine(bounds.left, bounds.top, bounds.right, bounds.top);
	DrawLine(bounds.left, bounds.bottom, bounds.right, bounds.bottom);
	DrawLine(bounds.left, bounds.top, bounds.left, bounds.bottom);
	DrawLine(bounds.right, bounds.top, bounds.right, bounds.bottom);
	DrawString(bounds.left + 1, bounds.top + 1, fText);
}

void Button::EventReceived(const Event *event)
{
	switch (event->what) {
		case EVT_MOUSE_DOWN:
			LockMouseFocus();
			fMouseDown = true;
			fOverButton = true;
			Invalidate();
			break;

		case EVT_MOUSE_UP:
			if (fOverButton)
				Invoke();

			fMouseDown = false;
			fOverButton = false;
			Invalidate();
			break;

		case EVT_MOUSE_ENTER:
			if (fMouseDown) {
				fOverButton = true;
				Invalidate();
			}

			break;

		case EVT_MOUSE_LEAVE:
			if (fMouseDown) {
				fOverButton = false;
				Invalidate();
			}

			break;

		default:
			Canvas::EventReceived(event);
	}
}

}
}

