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

class mycanvas : public Canvas {
public:
	mycanvas();

	virtual void Repaint(const Rect &dirtyRect);
	virtual void EventReceived(const Event*);

private:
	Button *fButton;
};

mycanvas::mycanvas()
	:	fButton(0)
{
	fButton = new Button("button");
	AddChild(fButton, Rect(5, 5, 50, 20));
	fButton->Show();
}

void mycanvas::Repaint(const Rect &dirtyRect)
{
	printf("mycanvas::Repaint: dirtyRect = ");dirtyRect.Dump();printf("\n");

	/* draw a red box around this canvas */
	SetPenColor(0xff0000); // red
	Rect bounds = GetBounds();
	printf("mycanvas::Repaint: bounds = ");bounds.Dump();printf("\n");
	DrawLine(bounds.left, bounds.top, bounds.right, bounds.top);
	DrawLine(bounds.right, bounds.top, bounds.right, bounds.bottom);
	DrawLine(bounds.left, bounds.bottom, bounds.right, bounds.bottom);
	DrawLine(bounds.left, bounds.top, bounds.left, bounds.bottom);

	SetPenColor(0); // black
	DrawString(10, 10, "this is a test");
	DrawString(20, 50, "this is another test");
}

void mycanvas::EventReceived(const Event *event)
{
	printf("mycanvas::EventReceived: Event %p, what %d\n", event, event->what);
}

const int kNumWindows = 16;

int main(int argc, char **argv)
{
	int left, top, width, height;
	Window *wins[kNumWindows];

	for(int i = 0; i < kNumWindows; i++) {
		left = rand() % 600;
		top = rand() % 400;
		width = rand() % (640 - left);
		height = rand() % (480 - top);

		Rect r;
		r.SetToSize(left, top, width, height);

		printf("creating window of rect ");r.Dump();printf("\n");

		Window *win = new Window(r);
		Canvas *canvas = new mycanvas();

		win->AddChild(canvas, win->GetCanvas()->GetBounds());
		win->Show();
		win->MakeFocus();
		canvas->SetBackgroundColor(0x00c4c0b8); // windows grey
		canvas->Invalidate();

		wins[i] = win;
	}

#if 0
	for(;;) {
		for(int i = 0; i < kNumWindows; i++) {
			sys_snooze(250000);

			wins[i]->Hide();
			sys_snooze(100000);
			wins[i]->Show();
		}
	}
#else
	for(;;)
		sys_snooze(1000000);
#endif
	return 0;
}

