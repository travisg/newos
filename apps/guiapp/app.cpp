/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <newos/errors.h>

#include <win/Window.h>
#include <win/Canvas.h>
#include <win/Event.h>
#include <win/Rect.h>
#include <win/Button.h>

using namespace os::gui;

class mycanvas : public Canvas {
public:
	mycanvas();

	virtual void Repaint(const Rect &dirtyRect);
	virtual void EventReceived(const Event*);

private:
	int fMouseX;
	int fMouseY;
};

mycanvas::mycanvas()
	:	fMouseX(0),
		fMouseY(0)
{
}

void mycanvas::Repaint(const Rect &dirtyRect)
{
	printf("mycanvas::Repaint: dirtyRect = ");dirtyRect.Dump();printf("\n");

#if 1
	/* draw a red box around this canvas */
	SetPenColor(0xff0000); // red
	Rect bounds = GetBounds();
	printf("mycanvas::Repaint: bounds = ");bounds.Dump();printf("\n");
	DrawLine(bounds.left, bounds.top, bounds.right, bounds.top);
	DrawLine(bounds.right, bounds.top, bounds.right, bounds.bottom);
	DrawLine(bounds.left, bounds.bottom, bounds.right, bounds.bottom);
	DrawLine(bounds.left, bounds.top, bounds.left, bounds.bottom);
#endif
	SetPenColor(0); // black
	char temp[128];
	sprintf(temp, "%d, %d", fMouseX, fMouseY);
	DrawString(20, 30, temp);
	DrawString(20, 50, "this is a test");
}

void mycanvas::EventReceived(const Event *event)
{
//	printf("mycanvas::EventReceived: Event %p, what %d\n", event, event->what);

	switch(event->what) {
		case EVT_MOUSE_MOVED:
			fMouseX = event->x;
			fMouseY = event->y;
			Invalidate(); // invalidate enough to cover the coordinate text
			break;
		default:
			Canvas::EventReceived(event);
	}
}

const int kNumWindows = 16;

int main(int argc, char **argv)
{
	int left, top, width, height;
	Window *wins[kNumWindows];

	for(int i = 0; i < kNumWindows; i++) {
		left = rand() % 600;
		top = rand() % 400;
		width = (rand() % (600 - left)) + 40;
		height = (rand() % (460 - top)) + 20;

		Rect r;
		r.SetToSize(left, top, width, height);

		printf("creating window of rect ");r.Dump();printf("\n");

		Window *win = new Window(r);
		Canvas *canvas = new mycanvas();
		Button *button = new Button("testbutton");

		win->AddChild(canvas, win->GetCanvas()->GetBounds());
		canvas->SetBackgroundColor(0x00c4c0b8); // windows grey
		canvas->AddChild(button, Rect(10, 10, 80, 20));
		button->SetBackgroundColor(0x00c4c0b8); // windows grey

		win->SetTitle("Test Title");
		win->Show();
		win->MakeFocus();

		wins[i] = win;
	}

#if 0
	for(;;) {
		for(int i = 0; i < kNumWindows; i++) {
			_kern_snooze(250000);

			wins[i]->Hide();
			_kern_snooze(100000);
			wins[i]->Show();
		}
	}
#else
	for(;;)
		_kern_snooze(1000000);
#endif
	return 0;
}

