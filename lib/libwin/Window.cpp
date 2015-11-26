#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>
#include <win/Window.h>
#include <win/Canvas.h>
#include <win/Event.h>
#include <win/protocol.h>
#include <win/Rect.h>
#include "Connection.h"

namespace os {
namespace gui {

/* special canvas class that is always placed first on any new window and draws the border */
class WindowBorderCanvas : public Canvas {
public:
    virtual void Repaint(const Rect &dirtyRect);
};

void WindowBorderCanvas::Repaint(const Rect &dirtyRect)
{
    // draw the border
    Rect bounds = GetBounds();

    SetPenColor(0); // black

    // bottom
    DrawLine(bounds.left + 1, bounds.bottom - 1, bounds.right - 1, bounds.bottom - 1);
    DrawLine(bounds.left, bounds.bottom, bounds.right, bounds.bottom);

    // right
    DrawLine(bounds.right - 1, bounds.bottom - 1, bounds.right - 1, bounds.top + 1);
    DrawLine(bounds.right, bounds.bottom, bounds.right, bounds.top);

    SetPenColor(0x00e0e0e0); // light grey

    // top
    DrawLine(bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.top + 1);
    DrawLine(bounds.left, bounds.top, bounds.right, bounds.top);

    // left
    DrawLine(bounds.left + 1, bounds.top + 1, bounds.left + 1, bounds.bottom - 1);
    DrawLine(bounds.left, bounds.top, bounds.left, bounds.bottom);
}

/* special canvas class that draws the title */
class TitleBarCanvas : public Canvas {
public:
    TitleBarCanvas();
    virtual ~TitleBarCanvas();
    virtual void Repaint(const Rect &dirtyRect);
    void SetTitle(const char *title);
private:
    char *fTitle;
};

TitleBarCanvas::TitleBarCanvas() : Canvas()
{
    fTitle = new char[1];
    fTitle[0] = 0;
}

TitleBarCanvas::~TitleBarCanvas()
{
    delete fTitle;
}

void TitleBarCanvas::SetTitle(const char *title)
{
    char *temp = new char[strlen(title)+1];
    char *old;

    strcpy(temp, title);
    old = fTitle;
    fTitle = temp;
    delete old;

    Invalidate();
}

void TitleBarCanvas::Repaint(const Rect &dirtyRect)
{
    SetPenColor(0xffffff); // white

    DrawString(5, 2, fTitle);
}

Window::Window(const Rect &loc)
    :   fShowLevel(0),
        fCanvasList(0)
{
    port_id windowServerPort;
    windowServerPort = _kern_port_find("window_server");
    if (windowServerPort < 0) {
        printf("couldn't connect to window server\n");
        return;
    }

    port_id localReplyPort = _kern_port_create(16, "client_syncport");
    fEventPort = _kern_port_create(16, "client_eventport");

    fConnection = new Connection(windowServerPort, localReplyPort);
    fConnection->WriteInt8(OP_CREATE_WINDOW);
    fConnection->WriteConnectionPort();
    fConnection->WriteInt32(0);         // Child of root window
    fConnection->WriteInt32(loc.left);
    fConnection->WriteInt32(loc.top);
    fConnection->WriteInt32(loc.right);
    fConnection->WriteInt32(loc.bottom);
    fConnection->WriteInt32(fEventPort);
    fConnection->WriteInt32(WINDOW_FLAG_TOPLEVEL);
    fConnection->Flush();
    fID = fConnection->ReadInt32();

    fLock = _kern_sem_create(1, "window_sem");

    thread_id tid = _kern_thread_create_thread("window_thread", &EventLoop, this);
    _kern_thread_resume_thread(tid);

    /* create the default border canvas */
    fBorderCanvas = new WindowBorderCanvas();
    Rect bounds = loc.Bounds();
    AddBorderChild(fBorderCanvas, bounds);
    fBorderCanvas->SetBackgroundColor(0x00c4c0b8);

    /* create a titlebar canvas */
    fTitleBarCanvas = new TitleBarCanvas();
    bounds = loc.Bounds();
    bounds.InsetBy(4, 4);
    bounds.bottom = bounds.top + 12 - 1;
    fBorderCanvas->AddChild(fTitleBarCanvas, bounds, WINDOW_FLAG_MOVABLE);
    fTitleBarCanvas->SetBackgroundColor(0xa0); // dark blue

    /* create a blank canvas to fill in the space in the middle of the window */
    fBackgroundCanvas = new Canvas();
    bounds = loc.Bounds();
    bounds.InsetBy(4, 4);
    bounds.top += 14;
    fBorderCanvas->AddChild(fBackgroundCanvas, bounds);
    fBackgroundCanvas->SetBackgroundColor(0x00c4c0b8); // windows grey

    /* invalidate all of the canvases we just built */
    fTitleBarCanvas->Invalidate();
    fBackgroundCanvas->Invalidate();
    fBorderCanvas->Invalidate();
}

Window::~Window()
{
    _kern_sem_acquire(fLock, 1);
    while (fCanvasList) {
        Canvas *child = fCanvasList;
        fCanvasList = fCanvasList->fWinListNext;
        fConnection->WriteInt8(OP_DESTROY_WINDOW);
        fConnection->WriteInt32(child->fID);
        fConnection->Flush();
        delete child;
    }

    fConnection->WriteInt8(OP_DESTROY_WINDOW);
    fConnection->WriteInt32(fID);
    fConnection->Flush();
    delete fConnection;

    _kern_sem_delete(fLock);
}


void Window::MakeFocus()
{
    fConnection->WriteInt8(OP_MAKE_FOCUS);
    fConnection->WriteInt32(fID);
    fConnection->Flush();
}

void Window::Flush()
{
    fConnection->Flush();
}

void Window::WaitEvent(Event *event)
{
    int ignore;
    printf("Window::WaitEvent: waiting on port %d\n", fEventPort);
    int err = _kern_port_read(fEventPort, &ignore, (void *)event, sizeof(Event));
    printf("Window::WaitEvent: got something from port %d, error %d\n", fEventPort, err);
}

void Window::AddBorderChild(Canvas *child, const Rect &childRect, window_flags flags)
{
    fConnection->WriteInt8(OP_CREATE_WINDOW);
    fConnection->WriteConnectionPort();
    fConnection->WriteInt32(fID);                   // My child
    fConnection->WriteInt32(childRect.left);
    fConnection->WriteInt32(childRect.top);
    fConnection->WriteInt32(childRect.right);
    fConnection->WriteInt32(childRect.bottom);
    fConnection->WriteInt32(fEventPort);
    fConnection->WriteInt32(flags);
    fConnection->Flush();
    child->fID = fConnection->ReadInt32();
    child->fWindow = this;
    child->fBounds = childRect.Bounds();
    child->Show();

    // Stick window in my canvas list
    child->fWinListNext = fCanvasList;
    child->fWinListPrev = &fCanvasList;
    if (fCanvasList)
        fCanvasList->fWinListPrev = &child->fWinListNext;

    fCanvasList = child;
}

Canvas *Window::GetCanvas()
{
    return fBackgroundCanvas;
}

void Window::AddChild(Canvas *child, const Rect &r, window_flags flags)
{
    fBackgroundCanvas->AddChild(child, r, flags);
}

void Window::RemoveChild(Canvas *child)
{
    Lock();
    fConnection->WriteInt8(OP_DESTROY_WINDOW);
    fConnection->WriteInt32(child->fID);
    fConnection->Flush();
    *child->fWinListPrev = child->fWinListNext;
    if (child->fWinListNext)
        child->fWinListNext->fWinListPrev = child->fWinListPrev;

    Unlock();
}

void Window::Lock()
{
    _kern_sem_acquire(fLock, 1);
}

void Window::Unlock()
{
    _kern_sem_release(fLock, 1);
}

void Window::Hide()
{
    if (--fShowLevel == 0) {
        fConnection->WriteInt8(OP_HIDE_WINDOW);
        fConnection->WriteInt32(fID);
        fConnection->Flush();
    }
}

void Window::Show()
{
    if (++fShowLevel == 1) {
        fConnection->WriteInt8(OP_SHOW_WINDOW);
        fConnection->WriteInt32(fID);
        fConnection->Flush();
    }
}

void Window::SetTitle(const char *title)
{
    static_cast<TitleBarCanvas *>(fTitleBarCanvas)->SetTitle(title);
}

Canvas* Window::FindChild(int id)
{
    for (Canvas *canvas = fCanvasList; canvas; canvas = canvas->fWinListNext)
        if (canvas->fID == id)
            return canvas;

    return 0;
}

void Window::DispatchEvent(Event *event)
{
    Canvas *canvas = FindChild(event->target);
    if (canvas)
        canvas->HandleEvent(event);
}

int Window::EventLoop(void *_window)
{
    Window *window = (Window*) _window;
    while (true) {
        Event event;

        event.what = -1;

        window->WaitEvent(&event);
        printf("Window::EventLoop: got event, what %d, target %d, x %d, y %d\n", event.what, event.target, event.x, event.y);
        if (event.what == EVT_QUIT) {
            delete window;
            _kern_exit(0);
        }

        window->Lock();
        window->DispatchEvent(&event);
        window->Unlock();
    }
}

void Window::Quit()
{
    Event event;
    event.what = EVT_QUIT;
    event.target = fID;

    _kern_port_write(fEventPort, 0, &event, sizeof(Event));
}

}
}

