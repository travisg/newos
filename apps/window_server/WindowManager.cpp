#include <sys/syscalls.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <win/Event.h>
#include <win/protocol.h>

using namespace os::gui;

#include "WindowManager.h"
#include "Renderer.h"
#include "Window.h"
#include "PS2Mouse.h"
#include "Renderer_vesa.h"

const int kReceiveBufferSize = 16384;
const int kMaxStrLen = 256;

WindowManager::WindowManager(Renderer *screenRenderer)
	:	fNextWindowID(1),
		fReceiveBufferSize(0),
		fReceiveBufferPos(0),
		fCurrentMouseFocus(0),
		fMouseFocusLocked(false),
		fInFocusLockedWindow(false),
		fScreenRenderer(screenRenderer)
{
	printf("WindowManager::WindowManager: constructor\n");

	fReceiveBuffer = (char*) malloc(kReceiveBufferSize);
	for (int i = 0; i < kMaxWindows; i++)
		fWindowArray[i] = 0;

	// Create the root window, which has an id of 0.
	fWindowArray[0] = new Window(0, -1, screenRenderer);
	Region screen;
	screen.Include(screenRenderer->Bounds());
	fWindowArray[0]->SetVisibleRegion(screen);
	fWindowArray[0]->SetColor(0x3a6ea5); // desktop blue
	fWindowArray[0]->Invalidate(fWindowArray[0]->LocalToScreen(screenRenderer->Bounds()));

	fCursorLock = sys_sem_create(1, "windowmanager_lock");

	thread_id tid;

	tid = sys_thread_create_thread("draw_thread", StartDispatchThread, this);
	printf("WindowManager::WindowManager: draw_thread id %d\n", tid);

	sys_thread_resume_thread(tid);

	tid = sys_thread_create_thread("mouse_thread", StartInputThread, this);
	sys_thread_resume_thread(tid);
}

WindowManager::~WindowManager()
{
	sys_sem_delete(fCursorLock);
}

Window* WindowManager::CreateWindow(Window *parent, const Rect &rect, int eventPort)
{
	while (fWindowArray[fNextWindowID % kMaxWindows] != 0)
		fNextWindowID++;

	Window *window = new Window(fNextWindowID, eventPort);
	fWindowArray[fNextWindowID % kMaxWindows] = window;
	fNextWindowID++;

	parent->AddChild(rect, window);
	return window;
}

void WindowManager::InvalidateMouseBoundries()
{
	fMouseBoundries.SetTo(-1, -1, -1, -1);
}


void WindowManager::DestroyWindow(Window *window)
{
	fWindowArray[window->ID() % kMaxWindows] = 0;
	delete window;
}

Window* WindowManager::LookupWindow(int id)
{
	Window *window = fWindowArray[id % kMaxWindows];
	if (window && window->ID() == id)
		return window;

	return 0;
}

Window* WindowManager::WindowAtPoint(int x, int y)
{
	Window *root = LookupWindow(0);
	return root->ChildAtPoint(x, y);
}

int WindowManager::StartDispatchThread(void *windowManager)
{
	((WindowManager*) windowManager)->DispatchThread();
	return 0;
}

void WindowManager::ReadServicePort(void *data, int size)
{
	int sizeRead = 0;
	while (sizeRead < size) {
		int sizeToRead = size - sizeRead;

		if (fReceiveBufferSize - fReceiveBufferPos < sizeToRead)
			sizeToRead = fReceiveBufferSize - fReceiveBufferPos;

		if (sizeToRead == 0) {
			int ignore;
			fReceiveBufferSize = sys_port_read(fServicePort, &ignore, fReceiveBuffer, kReceiveBufferSize);
			fReceiveBufferPos = 0;
			continue;
		}


		memcpy((void*) ((char*) data + sizeRead),
			(void*) (fReceiveBuffer + fReceiveBufferPos), sizeToRead);
		fReceiveBufferPos += sizeToRead;
		sizeRead += sizeToRead;
//		printf("WindowManager::ReadServicePort: size %d, sizeRead %d, sizeToRead %d, fReceiveBufferPos %d\n", size, sizeRead, sizeToRead, fReceiveBufferPos);
	}
}

void WindowManager::Respond(port_id port, void *data, int size)
{
	sys_port_write(port, 0, data, size);
}

int WindowManager::ReadInt32()
{
	int outval;
	ReadServicePort(&outval, 4);
	return outval;
}

short WindowManager::ReadInt16()
{
	short outval;
	ReadServicePort(&outval, 2);
	return outval;
}

char WindowManager::ReadInt8()
{
	char outval;
	ReadServicePort(&outval, 1);
	return outval;
}

void WindowManager::LockCursor()
{
	sys_sem_acquire(fCursorLock, 1);
}

void WindowManager::UnlockCursor()
{
	sys_sem_release(fCursorLock, 1);
}

void WindowManager::DispatchThread()
{
	printf("WindowManager::DispatchThread: entry\n");

    fServicePort = sys_port_create(256, "window_server");

	printf("WindowManager::DispatchThread: fServicePort %d\n", fServicePort);

	while (true) {
		printf("WindowManager::DispatchThread: waiting for message...\n");
		int opcode = ReadInt8();
		printf("WindowManager::DispatchThread: opcode %d\n", opcode);
		switch (opcode) {
			case OP_DRAW_LINE: {
				long windowID = ReadInt32();
				long x1 = ReadInt32();
				long y1 = ReadInt32();
				long x2 = ReadInt32();
				long y2 = ReadInt32();

				Window *targetWindow = LookupWindow(windowID);
				LockCursor();
				if (targetWindow)
					targetWindow->GC().DrawLine(x1, y1, x2, y2);
				UnlockCursor();

				break;
			}

			case OP_FILL_RECT: {
				long windowID = ReadInt32();
				long x1 = ReadInt32();
				long y1 = ReadInt32();
				long x2 = ReadInt32();
				long y2 = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				LockCursor();
				if (targetWindow)
					targetWindow->GC().FillRect(x1, y1, x2, y2);
				UnlockCursor();

				break;
			}

			case OP_SET_PEN_COLOR: {
				long windowID = ReadInt32();
				color888 color = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->GC().SetColor(color);

				break;
			}

			case OP_SET_BG_COLOR: {
				long windowID = ReadInt32();
				color888 color = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow) {
					targetWindow->SetColor(color);
					targetWindow->Invalidate(targetWindow->LocalToScreen(targetWindow->Bounds()));
				}

				break;
			}

			case OP_DRAW_STRING: {
				// Draw string
				long windowID = ReadInt32();
				long x = ReadInt32();
				long y = ReadInt32();

				char buf[kMaxStrLen + 1];
				char c;
				int index = 0;
				while (true) {
					c = ReadInt8();
					if (c == 0)
						break;

					if (index < kMaxStrLen)
						buf[index++] = c;
				}

				buf[index] = 0;

				printf("WindowManager::DispatchThread: OP_DRAW_STRING, x %d, y %d, '%s'\n", x, y, buf);

				Window *targetWindow = LookupWindow(windowID);
				LockCursor();
				if (targetWindow)
					targetWindow->GC().DrawString(x, y, buf);

				UnlockCursor();
				break;
			}

			case OP_DESTROY_WINDOW: {
				long windowID = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					DestroyWindow(targetWindow);

				InvalidateMouseBoundries();
				break;
			}

			case OP_SHOW_WINDOW: {
				long windowID = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->Show();

				InvalidateMouseBoundries();
				break;
			}

			case OP_HIDE_WINDOW: {
				long windowID = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->Hide();

				InvalidateMouseBoundries();
				break;
			}

			case OP_MAKE_FOCUS: {
				long windowID = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->MoveToFront();

				InvalidateMouseBoundries();
				break;
			}

			case OP_BEGIN_PAINT: {
				long windowID = ReadInt32();
				port_id responsePort = ReadInt32();
				Rect rect;
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->BeginPaint(rect);

				Respond(responsePort, &rect, sizeof(rect));
				break;
			}

			case OP_END_PAINT: {
				long windowID = ReadInt32();
				Window *targetWindow = LookupWindow(windowID);
				if (targetWindow)
					targetWindow->EndPaint();

				break;
			}

			case OP_CREATE_WINDOW: {
				port_id responsePort = ReadInt32();
				long parentWindow = ReadInt32();
				Rect rect;
				rect.left = ReadInt32();
				rect.top = ReadInt32();
				rect.right = ReadInt32();
				rect.bottom = ReadInt32();
				long eventPort = ReadInt32();
				printf("WindowManager::DispatchThread: OP_CREATE_WINDOW, eventPort %d, responsePort %d, parentWindow %d\n", eventPort, responsePort, parentWindow);

				Window *parent = LookupWindow(parentWindow);
				int id = -1;
				LockCursor();
				if (parent)
					id = CreateWindow(parent, rect, eventPort)->ID();
				UnlockCursor();

				Respond(responsePort, &id, sizeof(id));
				InvalidateMouseBoundries();
				break;
			}

			case OP_COPY_RECT: {
				long windowID = ReadInt32();
				Rect sourceRect;
				sourceRect.left = ReadInt32();
				sourceRect.top = ReadInt32();
				sourceRect.right = ReadInt32();
				sourceRect.bottom = ReadInt32();

				long destX = ReadInt32();
				long destY = ReadInt32();
				Rect destRect(sourceRect);
				destRect.OffsetTo(destX, destY);

				Window *targetWindow = LookupWindow(windowID);
				LockCursor();
				if (targetWindow) {
					Region invalidateRegion;
					if (targetWindow->InvalidRegion().Valid()) {
						// Parts of the window are damaged and can't be copied.
						Region copyRegion = targetWindow->ClipRegion();
						copyRegion.Exclude(targetWindow->InvalidRegion());
						targetWindow->GC().CopyRect(sourceRect, destRect,
							copyRegion, invalidateRegion);
					} else {
						targetWindow->GC().CopyRect(sourceRect, destRect,
							targetWindow->ClipRegion(), invalidateRegion);
					}

					// Draw parts of the window which have been "copied"
					// out from under another window.
					targetWindow->Invalidate(invalidateRegion);
				}
				UnlockCursor();

				break;
			}

			case OP_INVALIDATE: {
				long windowID = ReadInt32();
				Rect rect;
				rect.left = ReadInt32();
				rect.top = ReadInt32();
				rect.right = ReadInt32();
				rect.bottom = ReadInt32();

				Window *targetWindow = LookupWindow(windowID);
				LockCursor();
				if (targetWindow)
					targetWindow->Invalidate(targetWindow->LocalToScreen(rect));
				UnlockCursor();

				break;
			}

			case OP_LOCK_MOUSE_FOCUS:
				fMouseFocusLocked = true;
				break;

			default:
				printf("unrecognized drawing command %d\n", opcode);
		}
	}
}

void WindowManager::SetCursorPos(int x, int y)
{
	LockCursor();
	// a bit of a hack for now
	fScreenRenderer->SetCursorPosition(x, y);
	UnlockCursor();
}

int WindowManager::StartInputThread(void *_wm)
{
	((WindowManager*) _wm)->InputThread();
	return 0;
}

void WindowManager::InputThread()
{
	port_id event_port;

	// find the input server
retry:
	event_port = sys_port_find("input_event_port");
	if(event_port < 0) {
		sys_snooze(1000000);
		goto retry;
	}

	while (true) {
		Event event;
		int32 msg;
		sys_port_read(event_port, &msg, &event, sizeof(event));

		switch(event.what) {
			case EVT_MOUSE_MOVED:
				ProcessMouseEvent(event);
				break;
			case EVT_KEY_UP:
			case EVT_KEY_DOWN:
				ProcessKeyboardEvent(event);
				break;
		}
	}
}

void WindowManager::ProcessMouseEvent(const Event &event)
{
	static int cx = 160;
	static int cy = 100;
	static int buttons = 0;
	static int last_cx = cx;
	static int last_cy = cy;
	static int last_buttons = buttons;

	cx += event.x;
	cy += event.y;
	buttons = event.modifiers;
	if (last_cx != cx || last_cy != cy) {
		int delta_cx = cx - last_cx;
		int delta_cy = cy - last_cy;
		SetCursorPos(cx, cy);
		last_cx = cx;
		last_cy = cy;

		Window *oldMouseFocus = fCurrentMouseFocus;
		if (fCurrentMouseFocus == 0 || !fMouseBoundries.Contains(cx, cy)) {
			// Either there is no focus window, or we have moved out
			// of the current rectangle that is known to be in this window.
			// Check to see what window we are over.
			Window *focusWindow = WindowAtPoint(cx, cy);
			if (fMouseFocusLocked) {
				// Mouse focus is locked, don't actually switch focus windows
				if (focusWindow == fCurrentMouseFocus && !fInFocusLockedWindow) {
					// Inform the window with locked focus that the mouse has entered
					Rect screenFrame = fCurrentMouseFocus->LocalToScreen(
						fCurrentMouseFocus->Bounds());
					Event evt;
					evt.what = EVT_MOUSE_ENTER;
					evt.target = fCurrentMouseFocus->ID();
					evt.x = cx - screenFrame.left;
					evt.y = cy - screenFrame.top;
					fCurrentMouseFocus->PostEvent(&evt);
					fInFocusLockedWindow = true;
				} else if (focusWindow != fCurrentMouseFocus && fInFocusLockedWindow) {
					// Inform the window with the locked focus that the mouse has left.
					Event evt;
					evt.what = EVT_MOUSE_LEAVE;
					evt.target = oldMouseFocus->ID();
					fCurrentMouseFocus->PostEvent(&evt);
					fInFocusLockedWindow = false;
				}

				focusWindow->ClipRegion().FindRect(cx, cy, fMouseBoundries);
			} else {
				fCurrentMouseFocus = focusWindow;
				fCurrentMouseFocus->ClipRegion().FindRect(cx, cy, fMouseBoundries);
			}
		}

		if (oldMouseFocus == fCurrentMouseFocus) {
			// Post a mouse moved message to the current focus window
			Rect screenFrame = fCurrentMouseFocus->LocalToScreen(
				fCurrentMouseFocus->Bounds());

			Event evt;
			evt.what = EVT_MOUSE_MOVED;
			evt.target = fCurrentMouseFocus->ID();
			evt.x = cx - screenFrame.left;
			evt.y = cy - screenFrame.top;
			fCurrentMouseFocus->PostEvent(&evt);
		} else {
			// Inform the old window (if there is one), that the mouse is leaving
			if (oldMouseFocus) {
				Event evt;
				evt.what = EVT_MOUSE_LEAVE;
				evt.target = oldMouseFocus->ID();
				oldMouseFocus->PostEvent(&evt);
			}

			// Inform the new window that the mouse has entered
			Rect screenFrame = fCurrentMouseFocus->LocalToScreen(
				fCurrentMouseFocus->Bounds());

			Event evt;
			evt.what = EVT_MOUSE_ENTER;
			evt.target = fCurrentMouseFocus->ID();
			evt.x = cx - screenFrame.left;
			evt.y = cy - screenFrame.top;
			fCurrentMouseFocus->PostEvent(&evt);
		}
	}

	if (buttons != last_buttons && fCurrentMouseFocus) {
		// If the user has released the buttons, unlock the mouse focus
		if (buttons == 0 && fMouseFocusLocked) {
			InvalidateMouseBoundries();
			fMouseFocusLocked = false;
		}

		// Send a button message to the window
		Rect screenFrame = fCurrentMouseFocus->LocalToScreen(
			fCurrentMouseFocus->Bounds());

		Event evt;
		evt.what = buttons != 0 ? EVT_MOUSE_DOWN : EVT_MOUSE_UP;
		evt.target = fCurrentMouseFocus->ID();
		evt.x = cx - screenFrame.left;
		evt.y = cy - screenFrame.top;
		fCurrentMouseFocus->PostEvent(&evt);
		last_buttons = buttons;
	}
}

void WindowManager::ProcessKeyboardEvent(const Event &Event)
{


}

