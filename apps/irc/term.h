#ifndef _IRCTERM_H
#define _IRCTERM_H

class Term {
public:
	Term(int fd);
	~Term();

	void Lock();
	void Unlock();

	ssize_t Write(const char *buf);
	ssize_t WriteEscaped(const char *buf);

	void ClearScreen();
	void SaveCursor();
	void RestoreCursor();
	void SetScrollRegion(int y, int height);
	void SetCursor(int x, int y);

private:
	int mFd;
	sem_id mSem;

	static const char esc = 0x1b;
};

class TermWindow {
public:
	TermWindow(Term *term, int x, int y, int width, int height);
	~TermWindow();

	void Clear();
	void SetCursor(int x, int y);
	void Write(const char *buf, bool atLastLine);

private:
	void SetScrollRegion();

	Term *mTerm;
	int mX;
	int mY;
	int mWidth;
	int mHeight;
};

#endif

