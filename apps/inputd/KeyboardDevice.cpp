/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/types.h>
#include <unistd.h>
#include <win/Event.h>
using namespace os::gui;

#include "InputServer.h"
#include "KeyboardDevice.h"

#include <newos/key_event.h>

namespace {
const char unshifted_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', '{', '|', '}', '~', 0
};

const char caps_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    '`', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '{', '|', '}', '~', 0
};

const char shifted_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    ' ', '!', '"', '#', '$', '%', '&', '"',
    '(', ')', '*', '+', '<', '_', '>', '?',
    ')', '!', '@', '#', '$', '%', '^', '&',
    '*', '(', ':', ':', '<', '+', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '{', '|', '}', '^', '_',
    '~', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '{', '|', '}', '~', 0
};

const char shifted_caps_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    ' ', '!', '"', '#', '$', '%', '&', '"',
    '(', ')', '*', '+', '<', '_', '>', '?',
    ')', '!', '@', '#', '$', '%', '^', '&',
    '*', '(', ':', ':', '<', '+', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '{', '|', '}', '^', '_',
    '~', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', '{', '|', '}', '~', 0
};
}

int process_key_events(_key_event &kevent, Event &event)
{
    static bool shift = false;
    static bool caps = false;
    static bool ctrl = false;
    static bool alt = false;
    static bool win = false;
    static bool menu = false;
    char c;

    // deal with the common case first
    if (kevent.keycode < KEY_NONE) {
        const char *keymap;

        if (shift) {
            if (caps)
                keymap = shifted_caps_keymap;
            else
                keymap = shifted_keymap;
        } else {
            if (caps)
                keymap = caps_keymap;
            else
                keymap = unshifted_keymap;
        }

        c = keymap[kevent.keycode];
        if (c == 0)
            return -1;

        event.key = c;
    } else {
        /* extended keys */
        event.key = kevent.keycode; /* let the target figure this out */
        switch (kevent.keycode) {
            case KEY_CAPSLOCK:
                caps = !caps;
                break;
            case KEY_LSHIFT:
            case KEY_RSHIFT:
                if (kevent.modifiers & KEY_MODIFIER_DOWN)
                    shift = true;
                else
                    shift = false;
                break;
            case KEY_LCTRL:
            case KEY_RCTRL:
                if (kevent.modifiers & KEY_MODIFIER_DOWN)
                    ctrl = true;
                else
                    ctrl = false;
                break;
            case KEY_LALT:
            case KEY_RALT:
                if (kevent.modifiers & KEY_MODIFIER_DOWN)
                    alt = true;
                else
                    alt = false;
                break;
            case KEY_LWIN:
            case KEY_RWIN:
                if (kevent.modifiers & KEY_MODIFIER_DOWN)
                    win = true;
                else
                    win = false;
                break;
            case KEY_MENU:
                if (kevent.modifiers & KEY_MODIFIER_DOWN)
                    menu = true;
                else
                    menu = false;
                break;
        }
    }

    event.what = (kevent.modifiers & KEY_MODIFIER_DOWN) ? EVT_KEY_DOWN : EVT_KEY_UP;
    event.modifiers = (kevent.modifiers & KEY_MODIFIER_DOWN) ? EVT_MOD_KEYDOWN : EVT_MOD_KEYUP;
    event.modifiers |= shift ? EVT_MOD_SHIFT : 0;
    event.modifiers |= ctrl ? EVT_MOD_CTRL : 0;
    event.modifiers |= alt ? EVT_MOD_ALT : 0;
    event.modifiers |= win ? EVT_MOD_WIN : 0;
    event.modifiers |= menu ? EVT_MOD_MENU : 0;
    return 0;
}

KeyboardDevice::KeyboardDevice(int fd, InputServer *srv)
    :   InputDevice(fd, srv),
        mFd(fd),
        mSrv(srv)
{
}

KeyboardDevice::~KeyboardDevice()
{
}

int KeyboardDevice::_Run()
{
    for (;;) {
        _key_event kevent;
        ssize_t len;
        Event event;

        len = read(mFd, &kevent, sizeof(kevent));
        if (len < 0)
            break;

        if (process_key_events(kevent, event) < 0)
            continue;

        mSrv->PostEvent(event);
    }

    return 0;
}

