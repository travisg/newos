/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <newos/tty_priv.h>

#include "term.h"

Term::Term(int fd)
	: mFd(fd)
{
	mSem = _kern_sem_create(1, "term lock");

	tty_flags flags;
	flags.input_flags = TTY_FLAG_NLCR | TTY_FLAG_CRNL;
	flags.output_flags = TTY_FLAG_NLCR;

	ioctl(mFd, _TTY_IOCTL_SET_TTY_FLAGS, &flags, sizeof(flags));
}

Term::~Term()
{
	tty_flags flags;
	flags.input_flags = TTY_FLAG_DEFAULT_INPUT;
	flags.output_flags = TTY_FLAG_DEFAULT_OUTPUT;

	ioctl(mFd, _TTY_IOCTL_SET_TTY_FLAGS, &flags, sizeof(flags));

	Reset();

	_kern_sem_delete(mSem);
}

void Term::Lock()
{
	_kern_sem_acquire(mSem, 1);
}

void Term::Unlock()
{
	_kern_sem_release(mSem, 1);
}

void Term::ClearScreen()
{
	WriteEscaped("[2J");
	WriteEscaped("[H");
}

void Term::SaveCursor()
{
	WriteEscaped("7");
}

void Term::RestoreCursor()
{
	WriteEscaped("8");
}

void Term::Reset()
{
	WriteEscaped("c");
}

ssize_t Term::Write(const char *buf)
{
	return write(mFd, buf, strlen(buf));
}

ssize_t Term::WriteEscaped(const char *inbuf)
{
	char buf[4096];

	// XXX make safe
	sprintf(buf, "%c%s", esc, inbuf);

	return Write(buf);
}

void Term::SetScrollRegion(int y, int height)
{
	char buf[256];

	sprintf(buf, "[%d;%dr", y, y + height - 1);
	WriteEscaped(buf);
}

void Term::SetCursor(int x, int y)
{
	char buf[256];

	sprintf(buf, "[%d;%dH", y, x);
	WriteEscaped(buf);
}

void Term::ScrollUp(int count)
{
	char buf[256];

	sprintf(buf, "[%dM", count);
	WriteEscaped(buf);
}

void Term::ScrollDown(int count)
{
	char buf[256];

	sprintf(buf, "[%dL", count);
	WriteEscaped(buf);
}

TermWindow::TermWindow(Term *term, int x, int y, int width, int height)
	: mTerm(term),
	mX(x),
	mY(y),
	mWidth(width),
	mHeight(height)
{
}

TermWindow::~TermWindow()
{
}

void TermWindow::Clear()
{
	ScrollUp(mHeight);
}

void TermWindow::SetScrollRegion()
{
	mTerm->SetScrollRegion(mY, mHeight);
}

void TermWindow::ScrollUp(int count)
{
	mTerm->Lock();

	SetScrollRegion();

	mTerm->SaveCursor();
	mTerm->SetCursor(1, mY + mHeight - 1);

	mTerm->ScrollUp(count);

	mTerm->RestoreCursor();

	mTerm->Unlock();
}

void TermWindow::ScrollDown(int count)
{
	mTerm->Lock();

	SetScrollRegion();

	mTerm->SaveCursor();
	mTerm->SetCursor(1, mY);

	mTerm->ScrollDown(count);

	mTerm->RestoreCursor();

	mTerm->Unlock();
}

void TermWindow::Write(const char *buf, bool atLastLine)
{
	mTerm->Lock();

	SetScrollRegion();
	mTerm->SaveCursor();

	if(atLastLine)
		mTerm->SetCursor(1, mY + mHeight - 1);

	mTerm->Write(buf);

	mTerm->RestoreCursor();
	mTerm->Unlock();
}

void TermWindow::SetCursor(int x, int y)
{
	mTerm->Lock();

	if(x < 1)
		x = 1;
	if(x > mWidth)
		x = mWidth;
	if(y < 1)
		y = 1;
	if(y > mHeight)
		y = mHeight;

	mTerm->SetCursor(mX + x - 1, mY + y - 1);

	mTerm->Unlock();
}

