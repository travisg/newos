#include <sys/syscalls.h>
#include <stdio.h>
#include <win/Event.h>
#include "assert.h"
#include "Window.h"

using namespace os::gui;

Window::Window(int id, int eventPort, Renderer *renderer)
	:	fID(id),
		fNextSibling(0),
		fPreviousSibling(0),
		fChildList(0),
		fParent(0),
		fIsVisible(false),
		fEventPort(eventPort),
		fPaintMsgSent(false),
		fColor(0)
{
	if (renderer) {
		// I am being attached directly to a renderer.  My clip
		// region is the bounds of the renderer.
		fGC.SetRenderer(renderer);
		fFrame = renderer->Bounds();
		fVisibleRegion.Clear();
		fVisibleRegion.Include(fFrame);
		fClipRegion = fVisibleRegion;
		fGC.SetClipRegion(fClipRegion);
	}
}

Window::~Window()
{
	if (fParent)
		fParent->RemoveChild(this);
}

Window* Window::ChildAtPoint(int x, int y)
{
	// See what child is covering me at this point
	for (Window *child = fChildList; child; child = child->fNextSibling) {
		if (child->Frame().Contains(x, y))
			return child->ChildAtPoint(x - child->Frame().left, y - child->Frame().top);
	}

	return this;	// I guess that's me!
}

void Window::UpdateClipRegion()
{
	Region newClipRegion = fVisibleRegion;
	Rect myScreenFrame = LocalToScreen(Bounds());

	// Walk the child list.  It is ordered from front to back.
	for (Window *child = fChildList; child; child = child->fNextSibling) {
		// Ship hidden children (and effectively all their descendents).
		if (!child->IsVisible()) {
			Region empty;
			child->SetVisibleRegion(empty);
			continue;
		}

		// My children are obscured both by my siblings and theirs.
		// Create a new region.  Note that fClipRegion is initialized as
		// my visible region (that is, parts of me that aren't clipped by
		// my siblings). With iteration, each child window is excluded
		// from it, so this clips my children against each other.
		Region childClipRegion = newClipRegion;
		Rect childScreenFrame = LocalToScreen(child->Frame());
		childClipRegion.ConstrainTo(childScreenFrame);
		child->SetVisibleRegion(childClipRegion);

		// I am obscured by my child windows, remove them from my clip
		// region.
		newClipRegion.Exclude(childScreenFrame);
	}

	// Handle exposures.
	//	1. Invert the old clipping region to find
	//     which parts of the window were previously hidden.
	//	2. Intersect that with the new clipping region to find areas
	//     that have become visible.
	Region exposedRegion = fClipRegion;
	exposedRegion.Invert();
	exposedRegion.Intersect(newClipRegion);

	if (exposedRegion.Valid())
		Invalidate(exposedRegion);

	// Now set the new clip region for this window.
	fClipRegion = newClipRegion;
	fGC.SetClipRegion(fClipRegion);
	fGC.SetOrigin(myScreenFrame.left, myScreenFrame.top);
}

void Window::AddChild(const Rect &frame, Window *child)
{
	child->fFrame = frame;
	child->fGC.SetRenderer(fGC.GetRenderer());

	child->fNextSibling = fChildList;
	child->fPreviousSibling = &fChildList;
	if (fChildList)
		fChildList->fPreviousSibling = &child->fNextSibling;

	fChildList = child;
	child->fParent = this;
	if (child->IsVisible())
		UpdateClipRegion();
}

void Window::RemoveChild(Window *window)
{
	ASSERT(window->parent == this);
	ASSERT(window->fPreviousSibling);

	*window->fPreviousSibling = window->fNextSibling;
	fParent = 0;

	// Should remove all of its children

	UpdateClipRegion();
}

void Window::MoveToFront()
{
	fParent->RemoveChild(this);
	fParent->AddChild(fFrame, this);
	UpdateClipRegion();
}

void Window::SetVisibleRegion(const Region &region)
{
	fVisibleRegion = region;
	UpdateClipRegion();

	if (fInRedraw) {
		Region drawRegion = fCurrentRedrawRegion;
		drawRegion.Intersect(fClipRegion);
		fGC.SetClipRegion(drawRegion);
	} else
		fGC.SetClipRegion(fClipRegion);
}

const Region& Window::ClipRegion() const
{
	return fClipRegion;
}

void Window::MoveTo(long x, long y)
{
	fFrame.OffsetTo(x, y);
	UpdateClipRegion();
}

void Window::MoveBy(long x, long y)
{
	fFrame.OffsetBy(x, y);
	UpdateClipRegion();
}

void Window::ResizeTo(long width, long height)
{
	fFrame.right = fFrame.left + width;
	fFrame.bottom = fFrame.top + height;
	UpdateClipRegion();
}

void Window::Invalidate(const Region &region)
{
	// Erase background
	for (int rect = 0; rect < region.CountRects(); rect++) {
		const Rect &clipRect = region.RectAt(rect);
		fGC.GetRenderer()->FillRect(clipRect.left, clipRect.top, clipRect.right,
			clipRect.bottom, Color(), CSPACE_RGB888);
	}

	fInvalidRegion.Include(region);
	if (!fPaintMsgSent) {
		Event paintEvent;
		paintEvent.what = EVT_PAINT;
		PostEvent(&paintEvent);
		fPaintMsgSent = true;
	}
}

void Window::Invalidate(const Rect &rect)
{
	if (!fClipRegion.Intersects(rect))
		return; // this region isn't even visible, dont bother

	// Erase background
	Region eraseRegion(fClipRegion);
	Region invalRegion;
	invalRegion.Include(rect);
	eraseRegion.Intersect(invalRegion);
	for (int index = 0; index < eraseRegion.CountRects(); index++) {
		const Rect &clipRect = eraseRegion.RectAt(index);
		fGC.GetRenderer()->FillRect(clipRect.left, clipRect.top, clipRect.right,
			clipRect.bottom, Color(), CSPACE_RGB888);
	}

	// The rect is assumed to be in window coordinates
	fInvalidRegion.Include(rect);
	if (!fPaintMsgSent) {
		Event paintEvent;
		paintEvent.what = EVT_PAINT;
		PostEvent(&paintEvent);
		fPaintMsgSent = true;
	}

#if 0
	// walk my children and invalidate them if necessary
	for (Window *child = fChildList; child; child = child->fNextSibling) {
		child->Invalidate(rect);
	}
#endif
}

GraphicsContext& Window::GC()
{
	return fGC;
}

void Window::BeginPaint(Rect &out_invalidRect)
{
	fPaintMsgSent = false;
	fInRedraw = true;
	fCurrentRedrawRegion = fInvalidRegion;
	fInvalidRegion.Clear();

	Region drawRegion = fCurrentRedrawRegion;
	drawRegion.Intersect(fClipRegion);
	out_invalidRect = ScreenToLocal(drawRegion.Bounds());
	fGC.SetClipRegion(drawRegion);
}

void Window::EndPaint()
{
	fInRedraw = false;
}

bool Window::IsVisible() const
{
	return fIsVisible;
}

color888 Window::Color() const
{
	return fColor;
}

void Window::SetColor(color888 c)
{
	fColor = c;
}

void Window::Show()
{
	if (!fIsVisible) {
		fIsVisible = true;
		if (fParent)
			fParent->UpdateClipRegion();
	}
}

void Window::Hide()
{
	if (fIsVisible) {
		fIsVisible = false;
		if (fParent)
			fParent->UpdateClipRegion();
	}
}

void Window::PostEvent(Event *event)
{
	if(fEventPort < 0)
		return;

	event->target = fID;

	printf("Window::PostEvent: port %d ID %d, event->what %d\n", fEventPort, fID, event->what);

	sys_port_write(fEventPort, 0, event, sizeof(Event));
}

Rect Window::LocalToScreen(const Rect &inRect) const
{
	Rect outRect = inRect;
	for (const Window *window = this; window; window = window->fParent)
		outRect.OffsetBy(window->fFrame.left, window->fFrame.top);

	return outRect;
}

Rect Window::ScreenToLocal(const Rect &inRect) const
{
	Rect outRect = inRect;
	for (const Window *window = this; window; window = window->fParent)
		outRect.OffsetBy(-window->fFrame.left, -window->fFrame.top);

	return outRect;
}

void Window::DumpChildList(int level)
{
	for (int i = 0; i < level; i++)
		printf(" | ");

	Rect frame = Frame();
	printf(" + %d %d %d %d\n", frame.left, frame.top, frame.right, frame.bottom);
	for (Window *child = fChildList; child; child = child->fNextSibling)
		child->DumpChildList(level + 1);
}



