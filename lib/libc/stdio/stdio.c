#if !KERNEL
/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>  
#include <fcntl.h>
#include <newos/types.h>
#include <errno.h>

#define BUFFER_SIZE 1024

FILE *stdin;
FILE *stdout;
FILE *stderr;

int __stdio_init(void);	 /* keep the compiler happy, these two are not supposed */
int __stdio_deinit(void);/* to be called by anyone except crt0, and crt0 will change soon */

/* A stack of FILE's currently held by the user*/
FILE* __open_file_stack_top;
/* Semaphore used when adjusting the stack*/
sem_id __open_file_stack_sem_id;

static FILE *__create_FILE_struct(int fd, int flags)
{
    FILE *f;
    char name[32];

    /* Allocate the FILE*/
	f = (FILE *)malloc(sizeof(FILE));
	if(!f)
		return (FILE *)0;

    /* Allocate the buffer*/
    f->buf = (char *)malloc( BUFFER_SIZE *sizeof(char));
    if(!f->buf)
    {
        free(f);
        return (FILE *)0;
    }

    /* Create a semaphore*/
    sprintf(name, "%d :FILE", fd);
    f->sid = sys_sem_create(1, name);
    if(f->sid < 0)
    {
        free(f->buf);
        free(f);
        return (FILE *)0;
    }

    /* Fill in FILE values*/
    f->rpos = 0;
    f->buf_pos = 0;
    f->buf_size = BUFFER_SIZE ;
	f->fd = fd;
    f->flags = flags;

    /* Setup list*/
    f->next = __open_file_stack_top;

    /* Put the FILE in the list*/
    sys_sem_acquire(__open_file_stack_sem_id, 1);
    __open_file_stack_top = f;
    sys_sem_release(__open_file_stack_sem_id, 1);
	return f;
}

static int __delete_FILE_struct(int fd)
{
    FILE *fNode, *fPrev;
    
    /* Search for the FILE for the file descriptor */
    fPrev = (FILE*)0;
    fNode = __open_file_stack_top;
    while(fNode != (FILE*)0)
    {
        if(fNode->fd == fd)
        {
            break;
        }
        fPrev = fNode;
        fNode = fNode->next;
    }
    /* If it wasn't found return EOF*/
    if(fNode == (FILE*)0)
    {
        printf("Error: __delete_FILE_struct");
        return EOF;
    }

    /* free the FILE space/semaphore*/
    sys_close(fd);
    sys_sem_delete(fNode->sid);
    free(fNode->buf);
    free(fNode);

    /* Remove it from the list*/
    if(fNode == __open_file_stack_top)
    {
        sys_sem_acquire(__open_file_stack_sem_id, 1);
        __open_file_stack_top = __open_file_stack_top->next;
        sys_sem_release(__open_file_stack_sem_id, 1);
    }
    else
    {
        sys_sem_acquire(__open_file_stack_sem_id, 1);
        fPrev->next = fNode->next;
        sys_sem_release(__open_file_stack_sem_id, 1);
    }
    /* Free the space*/
    free(fNode);
    
    return 0;
}


int __stdio_init(void)
{
    /* Create semaphore*/
    __open_file_stack_sem_id = sys_sem_create(1, "__open_file_stack");
    /*initialize stack*/
    __open_file_stack_top = (FILE*)0;
	stdin = __create_FILE_struct(0, _STDIO_READ);
	stdout = __create_FILE_struct(1, _STDIO_WRITE);
	stderr = __create_FILE_struct(2, _STDIO_WRITE);

	return 0;
}

int __stdio_deinit(void)
{
    FILE *fNode, *fNext;
    
    /* Iterate through the list, freeing everything*/
    fNode = __open_file_stack_top;
    sys_sem_acquire(__open_file_stack_sem_id, 1);
    while(fNode != (FILE*)0)
    {
        fflush(fNode);
        fNext = fNode->next;
        free(fNode->buf);
        sys_sem_delete(fNode->sid);
        free(fNode);
        fNode = fNext;
    } 
    sys_sem_release(__open_file_stack_sem_id, 1);
    sys_sem_delete(__open_file_stack_sem_id);

	return 0;
}

FILE *fopen(const char *filename, const char *mode)
{
    FILE* f;
    int sys_flags;
    int flags;
    int fd;

    sys_flags = 0;

    if(!strcmp(mode, "r") || !strcmp(mode, "rb"))
    {
        sys_flags = O_RDONLY;
        flags = _STDIO_READ;
    }
    else if(!strcmp(mode, "w") || !strcmp(mode, "wb")) 
    {
        sys_flags = O_WRONLY | O_CREAT | O_TRUNC;
        flags = _STDIO_WRITE;
    }
    else if(!strcmp(mode, "a") || !strcmp(mode, "ab"))
    {
        sys_flags = O_WRONLY | O_CREAT | O_APPEND;
        flags = _STDIO_WRITE;
    }
    else if(!strcmp(mode, "r+") || !strcmp(mode, "rb+") || !strcmp(mode, "r+b"))
    {
        sys_flags = O_RDWR;
        flags = _STDIO_READ | _STDIO_WRITE;
    }
    else if(!strcmp(mode, "w+") || !strcmp(mode, "wb+") || !strcmp(mode, "w+b")) 
    {
        sys_flags = O_RDWR | O_CREAT | O_TRUNC;
        flags = _STDIO_READ | _STDIO_WRITE;
    }
    else if(!strcmp(mode, "a+") || !strcmp(mode, "ab+") || !strcmp(mode, "a+b")) 
    {
        sys_flags = O_RDWR | O_CREAT | O_APPEND;
        flags = _STDIO_READ | _STDIO_WRITE;
    }
    else
    {
        return (FILE*)0;
    }

    fd = sys_open(filename, 0, sys_flags);
    if(fd < 0)
    {
        return (FILE*)0;
    }
    
    f = __create_FILE_struct(fd, flags);
    if(f == (FILE*)0)
    {
        sys_close(fd);
    }

    return f;
}

int fclose(FILE *stream)
{
    int err;
    err = fflush(stream);

    __delete_FILE_struct(stream->fd);
    return err;
}

int fflush(FILE *stream)
{
    int err;
    err = 0;

    if(stream == (FILE*)0)
    {
        FILE* node;
        node = __open_file_stack_top;
        err = 0;
        while(node != (FILE*)0)
        {
            int e = fflush(node);
            if(e == EOF)
                err = EOF;
            node = node->next;
        }
    }
    else
    {
        if(stream->flags & _STDIO_WRITE )
        {
            sys_sem_acquire(stream->sid, 1);
            err = sys_write(stream->fd, stream->buf, -1, stream->buf_pos);
            sys_sem_release(stream->sid, 1);
            stream->buf_pos = 0;
        }
    }
    
    if(err < 0)
    {
        errno = EIO;
        stream->flags |= _STDIO_ERROR;
        return EOF;
    }

    return err;
}

int printf(const char *fmt, ...)
{
	va_list args;
	int i;

    va_start(args, fmt);
    sys_sem_acquire(stdout->sid, 1);
	i = vfprintf(stdout, fmt, args);
    sys_sem_release(stdout->sid, 1);
	va_end(args);

	return i;
}

int fprintf(FILE *stream, char const *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	sys_sem_acquire(stream->sid, 1);
    i = vfprintf(stream, fmt, args);
    sys_sem_release(stream->sid, 1);    
	va_end(args);

	return i;
}

int feof(FILE *stream)
{
    int i = 0;
	sys_sem_acquire(stream->sid, 1);
    i = stream->flags & _STDIO_EOF;
    sys_sem_release(stream->sid, 1);    
    return i;
}

int ferror (FILE *stream)
{
    int i = 0;
	sys_sem_acquire(stream->sid, 1);
    i = stream->flags & _STDIO_ERROR;
    sys_sem_release(stream->sid, 1);
    return i;
}

int getchar(void)
{
	char c;

	sys_read(stdin->fd, &c, -1, 1);
	return c;
}

char* fgets(char* str, int n, FILE * stream)
{
    unsigned char* tmp;
    int i = n-1;
    tmp = str;

	sys_sem_acquire(stream->sid, 1);

    for(;i > 0; i--)
    {
        int c;
        
        if (stream->flags & _STDIO_EOF)
        {
            break;
        }
        if (stream->rpos >= stream->buf_pos) 
        {

            int len = sys_read(stream->fd, stream->buf, -1, stream->buf_size);

            if (len==0) 
            {
                stream->flags |= _STDIO_EOF;
                break;
            } 
            else if (len < 0) 
            {
                stream->flags |= _STDIO_ERROR;
                break;
            }
            stream->rpos=0;
            stream->buf_pos=len;
        }
        c = stream->buf[stream->rpos++];
        
        *tmp++ = c;
        if(c == '\n')
            break;
    }

    sys_sem_release(stream->sid, 1);


    *tmp = '\0';
    return str;
}

int fgetc(FILE *stream)
{
    int c;
    sys_sem_acquire(stream->sid, 1);
    if (stream->rpos >= stream->buf_pos) 
    {
        int len = sys_read(stream->fd, stream->buf, -1, stream->buf_size);
    
        if (len==0) 
        {
            stream->flags |= _STDIO_EOF;
            return EOF;
        } 
        else if (len < 0) 
        {
            stream->flags |= _STDIO_ERROR;
            return EOF;
        }
        stream->rpos=0;
        stream->buf_pos=len;
    }
    c = stream->buf[stream->rpos++];
    sys_sem_release(stream->sid, 1);    
    return c;
}

int scanf(char const *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	sys_sem_acquire(stdout->sid, 1);
	i = vfscanf(stdout, fmt, args);
	sys_sem_release(stdout->sid, 1);    
	va_end(args);

	return i;
}
int fscanf(FILE *stream, char const *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	sys_sem_acquire(stream->sid, 1);
	i = vfscanf(stream, fmt, args);
	sys_sem_release(stream->sid, 1);    
	va_end(args);

	return i;
}


#endif

