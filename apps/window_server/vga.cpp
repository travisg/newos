/* $Id$
**
** Copyright 1998 Brian J. Swetland
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions, and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions, and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "vga.h"

#include <i386/io.h>

const int MiscOutputReg = 0x3c2;
const int DataReg = 0x3c0;
const int AddressReg = 0x3c0;

const char mode13[][32] = {
  { 0x03, 0x01, 0x0f, 0x00, 0x0e },            /* 0x3c4, index 0-4*/
  { 0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9c, 0x0e, 0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3,
    0xff },                                    /* 0x3d4, index 0-0x18*/
  { 0, 0, 0, 0, 0, 0x40, 0x05, 0x0f, 0xff },   /* 0x3ce, index 0-8*/ 
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
    0x41, 0, 0x0f, 0, 0 }                      /* 0x3c0, index 0-0x14*/
};

#define Vga256                   0x13   // 320x200x256
#define TextMode                 0x03   // 80x25 text mode
#define PaletteMask              0x3C6  // bit mask register
#define PaletteRegisterRead      0x3C7  // read index
#define PaletteRegisterWrite     0x3C8  // write index
#define PaletteData              0x3C9  // send/receive data here
#define RomCharSegment           0xF000 
#define RomCharOffset            0xFA6E 
#define CharWidth                8
#define CharHeight               8

void SetPalette(int color, int red, int green, int blue)
{

  outb(0xFF, PaletteMask);
  outb(color, PaletteRegisterWrite);
  outb(red, PaletteData);
  outb(green, PaletteData);
  outb(blue, PaletteData);
}      

int InitVGA()
{
  int i;

  outb_p(0x63, MiscOutputReg);
  outb_p(0x00, 0x3da);

  for (i=0; i < 5; i++) {
    outb_p(i, 0x3c4);
    outb_p(mode13[0][i], 0x3c4+1);
  }

  outw_p(0x0e11, 0x3d4);

  for (i=0; i < 0x19; i++) {
    outb_p(i, 0x3d4);
    outb_p(mode13[1][i], (0x3d4 + 1));
  }  

  for (i=0; i < 0x9; i++) {
    outb_p(i, 0x3ce);
    outb_p(mode13[2][i], (0x3ce + 1));
  }

  inb_p(0x3da);

  for (i=0; i < 0x15; i++) {
    inw(DataReg);
    outb_p(i, AddressReg);
    outb_p(mode13[3][i], DataReg);
  }

  outb_p(0x20, 0x3c0);

  return 1;
}

