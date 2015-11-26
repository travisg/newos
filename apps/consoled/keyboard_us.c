/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <newos/tty_priv.h>
#include <newos/key_event.h>

#include "consoled.h"

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

int process_key_events(_key_event *kevents, int num_events, char *out_buf, int buf_len, int keyboard_fd)
{
    int i;
    static bool numlock = false;
    static bool shift = false;
    static bool caps = false;
    static bool ctrl = false;
    // dont really need to track these
    static bool alt = false;
    static bool win = false;
    static bool menu = false;
    static const char *keymap = unshifted_keymap; // track the status of shift + caps
    static int leds = 0;
    int buf_pos = 0;
    char c;

    for (i=0; i<num_events; i++) {
        bool reset_keymap = false;
//      printf("process_key_events: event %d: keycode 0x%x modifiers 0x%x\n", i, kevents[i].keycode, kevents[i].modifiers);

        if (kevents[i].modifiers & KEY_MODIFIER_DOWN) {
            // deal with the common case first
            if (kevents[i].keycode < KEY_NONE) {
                c = keymap[kevents[i].keycode];
                if (c == 0)
                    continue;

                if (ctrl) {
                    if (c == ' ') {
                        c = 0;
                    } else {
                        char temp = toupper(c);
                        if (temp >= 'A' && temp <= ']')
                            c = temp - 'A' + 1;
                    }
                }

                out_buf[buf_pos++] = c;
            } else {
                /* extended keys */
                char buf[16];
                int count = 0;
                int j;

                switch (kevents[i].keycode) {
                    case KEY_LSHIFT:
                    case KEY_RSHIFT:
                        shift = true;
                        reset_keymap = true;
                        break;
                    case KEY_LCTRL:
                    case KEY_RCTRL:
                        ctrl = true;
                        break;
                    case KEY_LALT:
                    case KEY_RALT:
                        alt = true;
                        break;
                    case KEY_LWIN:
                    case KEY_RWIN:
                        win = true;
                        break;
                    case KEY_MENU:
                        menu = true;
                        break;
                    case KEY_CAPSLOCK:
                        caps = !caps;
                        reset_keymap = true;
                        leds = caps ? (leds | KEY_LED_CAPS) : (leds & ~KEY_LED_CAPS);
                        ioctl(keyboard_fd, _KEYBOARD_IOCTL_SET_LEDS, &leds, sizeof(leds));
                        break;
                    case KEY_PAD_NUMLOCK:
                        numlock = !numlock;
                        leds = numlock ? (leds | KEY_LED_NUM) : (leds & ~KEY_LED_NUM);
                        ioctl(keyboard_fd, _KEYBOARD_IOCTL_SET_LEDS, &leds, sizeof(leds));
                        break;
                    case KEY_RETURN:
                    case KEY_PAD_ENTER:
                        buf[count++] = '\n';
                        break;
                    case KEY_BACKSPACE:
                        buf[count++] = 0x8;
                        break;
                    case KEY_TAB:
                        buf[count++] = '\t';
                        break;
                    case KEY_PAD_DIVIDE:
                        buf[count++] = '/';
                        break;
                    case KEY_PAD_MULTIPLY:
                        buf[count++] = '*';
                        break;
                    case KEY_PAD_MINUS:
                        buf[count++] = '-';
                        break;
                    case KEY_PAD_PLUS:
                        buf[count++] = '+';
                        break;
                    case KEY_PAD_PERIOD:
                        buf[count++] = '.';
                        break;
                    case KEY_PAD_0:
                        if (numlock) buf[count++] = '0';
                        break;
                    case KEY_PAD_1:
                        if (numlock) buf[count++] = '1';
                        break;
                    case KEY_PAD_2:
                        if (numlock) buf[count++] = '2';
                        break;
                    case KEY_PAD_3:
                        if (numlock) buf[count++] = '3';
                        break;
                    case KEY_PAD_4:
                        if (numlock) buf[count++] = '4';
                        break;
                    case KEY_PAD_5:
                        if (numlock) buf[count++] = '5';
                        break;
                    case KEY_PAD_6:
                        if (numlock) buf[count++] = '6';
                        break;
                    case KEY_PAD_7:
                        if (numlock) buf[count++] = '7';
                        break;
                    case KEY_PAD_8:
                        if (numlock) buf[count++] = '8';
                        break;
                    case KEY_PAD_9:
                        if (numlock) buf[count++] = '9';
                        break;
                }
                for (j = 0; j < count; j++) {
                    out_buf[buf_pos++] = buf[j];
                }
            }
        } else if (kevents[i].modifiers & KEY_MODIFIER_UP) {
            switch (kevents[i].keycode) {
                case KEY_LSHIFT:
                case KEY_RSHIFT:
                    shift = false;
                    reset_keymap = true;
                    break;
                case KEY_LCTRL:
                case KEY_RCTRL:
                    ctrl = false;
                    break;
                case KEY_LALT:
                case KEY_RALT:
                    alt = false;
                    break;
                case KEY_LWIN:
                case KEY_RWIN:
                    win = false;
                    break;
                case KEY_MENU:
                    menu = false;
                    break;
            }
        }

        // the shift or caps state changed, reset the keymap
        if (reset_keymap) {
            if (shift) {
                if (caps) keymap = shifted_caps_keymap;
                else     keymap = shifted_keymap;
            } else {
                if (caps) keymap = caps_keymap;
                else     keymap = unshifted_keymap;
            }
        }
    }
    return buf_pos;
}


