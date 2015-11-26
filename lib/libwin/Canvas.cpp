#include <stdio.h>
#include <win/Canvas.h>
#include <win/Rect.h>
#include <win/Window.h>
#include <win/Event.h>
#include <win/protocol.h>
#include "Connection.h"

namespace os {
namespace gui {

Canvas::Canvas()
    :   fID(-1),
        fWindow(0),
        fInPaint(false),
        fShowLevel(0)
{
}

Canvas::~Canvas()
{
}

void Canvas::DrawLine(int x1, int y1, int x2, int y2)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_DRAW_LINE);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(x1);
    fWindow->fConnection->WriteInt32(y1);
    fWindow->fConnection->WriteInt32(x2);
    fWindow->fConnection->WriteInt32(y2);
    fWindow->fConnection->EndCommand();
}

void Canvas::FillRect(const Rect &r)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_FILL_RECT);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(r.left);
    fWindow->fConnection->WriteInt32(r.top);
    fWindow->fConnection->WriteInt32(r.right);
    fWindow->fConnection->WriteInt32(r.bottom);
    fWindow->fConnection->EndCommand();
}

void Canvas::SetPenColor(color888 c)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_SET_PEN_COLOR);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(c);
    fWindow->fConnection->EndCommand();
}

void Canvas::SetBackgroundColor(color888 c)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_SET_BG_COLOR);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(c);
    fWindow->fConnection->EndCommand();
}

void Canvas::DrawString(int x, int y, const char *str)
{
    if (fWindow == 0)
        return;

    printf("Canvas::DrawString: x %d, y %d, str '%s' (ID %d)\n", x, y, str, fID);

    fWindow->fConnection->WriteInt8(OP_DRAW_STRING);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(x);
    fWindow->fConnection->WriteInt32(y);
    while (*str)
        fWindow->fConnection->WriteInt8(*str++);

    fWindow->fConnection->WriteInt8(0);
    fWindow->fConnection->EndCommand();

    printf("Canvas::DrawString: done\n");
}

void Canvas::CopyRect(const Rect &r, int newLeft, int newTop)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_COPY_RECT);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(r.left);
    fWindow->fConnection->WriteInt32(r.top);
    fWindow->fConnection->WriteInt32(r.right);
    fWindow->fConnection->WriteInt32(r.bottom);
    fWindow->fConnection->WriteInt32(newLeft);
    fWindow->fConnection->WriteInt32(newTop);
    fWindow->fConnection->EndCommand();
}

void Canvas::CopyRect(const Rect &r, const Rect &r2)
{
    CopyRect(r, r2.left, r2.top);
}

void Canvas::Hide()
{
    if (fWindow == 0)
        return;

    if (--fShowLevel == 0) {
        fWindow->fConnection->WriteInt8(OP_HIDE_WINDOW);
        fWindow->fConnection->WriteInt32(fID);
        fWindow->fConnection->Flush();
    }
}

void Canvas::Show()
{
    if (fWindow == 0)
        return;

    if (++fShowLevel == 1) {
        fWindow->fConnection->WriteInt8(OP_SHOW_WINDOW);
        fWindow->fConnection->WriteInt32(fID);
        fWindow->fConnection->Flush();
    }
}

void Canvas::BeginPaint(Rect &r)
{
    if (fInPaint)
        return;

    fInPaint = true;
    fWindow->fConnection->WriteInt8(OP_BEGIN_PAINT);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteConnectionPort();
    fWindow->fConnection->Flush();

    r.left = fWindow->fConnection->ReadInt32();
    r.top = fWindow->fConnection->ReadInt32();
    r.right = fWindow->fConnection->ReadInt32();
    r.bottom = fWindow->fConnection->ReadInt32();
}

void Canvas::EndPaint()
{
    if (!fInPaint)
        return;

    fInPaint = false;
    fWindow->fConnection->WriteInt8(OP_END_PAINT);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->Flush();
}

void Canvas::AddChild(Canvas *child, const Rect &childRect, window_flags flags)
{
    if (fWindow == 0)
        return;

    fWindow->fConnection->WriteInt8(OP_CREATE_WINDOW);
    fWindow->fConnection->WriteConnectionPort();
    fWindow->fConnection->WriteInt32(fID);                  // My child
    fWindow->fConnection->WriteInt32(childRect.left);
    fWindow->fConnection->WriteInt32(childRect.top);
    fWindow->fConnection->WriteInt32(childRect.right);
    fWindow->fConnection->WriteInt32(childRect.bottom);
    fWindow->fConnection->WriteInt32(fWindow->fEventPort);
    fWindow->fConnection->WriteInt32(flags);
    fWindow->fConnection->Flush();
    child->fID = fWindow->fConnection->ReadInt32();
    child->fWindow = fWindow;
    child->fBounds = childRect.Bounds();

    // Stick window in the window's canvas list
    child->fWinListNext = fWindow->fCanvasList;
    child->fWinListPrev = &fWindow->fCanvasList;
    if (fWindow->fCanvasList)
        fWindow->fCanvasList->fWinListPrev = &child->fWinListNext;

    fWindow->fCanvasList = child;

    child->Show();
}

void Canvas::RemoveChild(Canvas *child)
{
    fWindow->fConnection->WriteInt8(OP_DESTROY_WINDOW);
    fWindow->fConnection->WriteInt32(child->fID);
    fWindow->fConnection->Flush();
    *child->fWinListPrev = child->fWinListNext;
}

void Canvas::HandleEvent(Event *event)
{
    switch (event->what) {
        case EVT_PAINT: {
            Rect dirtyRect;
            BeginPaint(dirtyRect);
            Repaint(dirtyRect);
            EndPaint();
            break;
        }

        default:
            EventReceived(event);
    }
}

Window* Canvas::GetWindow() const
{
    return fWindow;
}

Rect Canvas::GetBounds() const
{
    return fBounds;
}

void Canvas::Invalidate()
{
    fWindow->fConnection->WriteInt8(OP_INVALIDATE);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(fBounds.left);
    fWindow->fConnection->WriteInt32(fBounds.top);
    fWindow->fConnection->WriteInt32(fBounds.right);
    fWindow->fConnection->WriteInt32(fBounds.bottom);
    fWindow->fConnection->Flush();
}

void Canvas::Invalidate(const Rect &r)
{
    fWindow->fConnection->WriteInt8(OP_INVALIDATE);
    fWindow->fConnection->WriteInt32(fID);
    fWindow->fConnection->WriteInt32(r.left);
    fWindow->fConnection->WriteInt32(r.top);
    fWindow->fConnection->WriteInt32(r.right);
    fWindow->fConnection->WriteInt32(r.bottom);
    fWindow->fConnection->Flush();
}

void Canvas::Repaint(const Rect &dirtyRect)
{
}

void Canvas::EventReceived(const Event *)
{
}

void Canvas::LockMouseFocus()
{
    fWindow->fConnection->WriteInt8(OP_LOCK_MOUSE_FOCUS);
    fWindow->fConnection->Flush();
}

}
}

