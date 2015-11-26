#ifndef _BUTTON_H
#define _BUTTON_H

#include <win/Canvas.h>

namespace os {
namespace gui {

class Button : public Canvas {
public:
    Button(const char *text);
    virtual ~Button();
    virtual void Invoke();

    virtual void Repaint(const Rect &dirtyRect);
    virtual void EventReceived(const Event*);

private:
    char *fText;
    bool fMouseDown;
    bool fOverButton;
};

}
}

#endif

