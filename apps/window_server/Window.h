#ifndef _WINDOW_H
#define _WINDOW_H

#include "Region.h"
#include "GraphicsContext.h"
#include <win/Event.h>
#include <win/WindowFlags.h>

using namespace os::gui;

class Window {
public:

    Window(int id, port_id eventPort, Renderer *renderer = 0);
    ~Window();
    void AddChild(const Rect& frame, Window *window, window_flags flags = WINDOW_FLAG_NONE);
    void RemoveChild(Window *window);
    void MoveToFront();
    inline int ID() const;

    Window *ChildAtPoint(int x, int y);

    Rect LocalToScreen(const Rect&) const;
    Rect ScreenToLocal(const Rect&) const;

    void SetVisibleRegion(const Region&);
    inline const Rect& Frame() const;
    inline Rect Bounds() const;

    const Region& InvalidRegion() const;
    const Region& ClipRegion() const;
    void Invalidate(const Region&);
    void Invalidate(const Rect&); // rect in screen coords
    void BeginPaint(Rect &out_invalidRect);
    void EndPaint();
    GraphicsContext& GC();
    void ResetGC();
    bool IsVisible() const;
    color888 Color() const;
    void SetColor(color888);
    void Show();
    void Hide();
    void PostEvent(Event*);

    void DumpChildList(int level = 0);

    void MoveTo(long, long);
    void MoveBy(long, long);
    void ResizeTo(long, long);

    inline window_flags Flags() const;
    inline Window *GetTopLevelWindow() const;

private:
    void UpdateClipRegion();

    int fID;
    Window *fNextSibling;
    Window **fPreviousSibling;
    Window *fChildList;
    Window *fParent;
    Window *fToplevelWindow;
    window_flags fFlags;

    Region fInvalidRegion;
    Region fCurrentRedrawRegion;

    // The visible region represents what part of this window is not
    // obscured by siblings of my parent.  I maintain this
    // when recomputing clipping for my children.
    Region fVisibleRegion;

    // The clip region is the visible region, minus parts of my window
    // that are obscured by my children.
    Region fClipRegion;

    Rect fFrame;
    GraphicsContext fGC;
    bool fIsVisible;
    bool fInRedraw;
    port_id fEventPort;
    bool fPaintMsgSent;
    color888 fColor;
};

inline int Window::ID() const
{
    return fID;
}

inline const Rect& Window::Frame() const
{
    return fFrame;
}

inline Rect Window::Bounds() const
{
    Rect rect(fFrame);
    rect.OffsetTo(0, 0);
    return rect;
}

inline window_flags Window::Flags() const
{
    return fFlags;
}

inline Window *Window::GetTopLevelWindow() const
{
    return fToplevelWindow;
}

inline const Region& Window::InvalidRegion() const
{
    return fInvalidRegion;
}




#endif

