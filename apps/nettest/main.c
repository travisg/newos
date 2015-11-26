/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <newos/errors.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <socket/socket.h>

static int fd;
static sem_id exit_sem;

int nettest1(void);
int nettest2(void);
int nettest3(void);
int nettest4(void);
int nettest5(void);
int nettest6(void);

static int read_thread(void *args)
{
    char buf[1024];
    ssize_t len;
    int i;

    for (;;) {
        len = socket_read(fd, buf, sizeof(buf));
        if (len < 0) {
            printf("\nsocket_read returns %ld\n", (long)len);
            break;
        }

        for (i=0; i<len; i++)
            printf("%c", buf[i]);
    }

    _kern_sem_release(exit_sem, 1);

    return 0;
}

static int write_thread(void *args)
{
    char c;
    ssize_t len;

    for (;;) {
        c = getchar();
        len = socket_write(fd, &c, sizeof(c));
        if (len < 0) {
            printf("\nsocket_write returns %ld\n", len);
            break;
        }
    }

    _kern_sem_release(exit_sem, 1);

    return 0;
}

int nettest1(void)
{
    int err;
    sockaddr addr;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
//  addr.port = 19;
//  NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73);
//  addr.port = 23;
//  NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(192,168,0,3);
    addr.port = 6667;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(209,131,227,242);

    err = socket_connect(fd, &addr);
    printf("socket_connect returns %d\n", err);
    if (err < 0)
        return err;

    exit_sem = _kern_sem_create(0, "exit");

    _kern_thread_resume_thread(_kern_thread_create_thread("read_thread", &read_thread, NULL));
    _kern_thread_resume_thread(_kern_thread_create_thread("write_thread", &write_thread, NULL));

    for (;;)
        _kern_sem_acquire(exit_sem, 1);

    return 0;
}

int nettest2(void)
{
    int err;
    sockaddr addr;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
    addr.port = 9;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73);

    err = socket_connect(fd, &addr);
    printf("socket_connect returns %d\n", err);
    if (err < 0)
        return err;

    for (;;) {
        char buf[4096];
        socket_write(fd, buf, sizeof(buf));
    }
    return 0;
}

int nettest3(void)
{
    int err;
    sockaddr addr;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
    addr.port = 1900;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73);

    err = socket_connect(fd, &addr);
    printf("socket_connect returns %d\n", err);
    if (err < 0)
        return err;

    for (;;) {
        char buf[4096];
        ssize_t len;

        len = socket_read(fd, buf, sizeof(buf));
        printf("socket_read returns %ld\n", len);
        if (len <= 0)
            break;
    }

    err = socket_close(fd);
    printf("socket_close returns %d\n", err);

    return 0;
}

int nettest4(void)
{
    int err;
    sockaddr addr;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
    addr.port = 1900;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73);

    err = socket_connect(fd, &addr);
    printf("socket_connect returns %d\n", err);
    if (err < 0)
        return err;

    usleep(5000000);

    err = socket_close(fd);
    printf("socket_close returns %d\n", err);

    return 0;
}

int nettest5(void)
{
    int err;
    sockaddr addr;
    int new_fd;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
    addr.port = 1900;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(0,0,0,0);

    err = socket_bind(fd, &addr);
    printf("socket_bind returns %d\n", err);
    if (err < 0)
        return 0;

    err = socket_listen(fd);
    printf("socket_listen returns %d\n", err);
    if (err < 0)
        return 0;

    new_fd = socket_accept(fd, &addr);
    printf("socket_accept returns %d\n", new_fd);
    if (new_fd < 0)
        return 0;

    err = socket_write(new_fd, "hello world!\n", strlen("hello world!\n"));
    printf("socket_write returns %d\n", err);

    printf("sleeping for 5 seconds\n");
    usleep(5000000);

    printf("closing fd %d\n", new_fd);
    socket_close(new_fd);
    printf("closing fd %d\n", fd);
    socket_close(fd);

    return 0;
}

int nettest6(void)
{
    int err;
    sockaddr addr;
    int new_fd;

    fd = socket_create(SOCK_PROTO_TCP, 0);
    printf("created socket, fd %d\n", fd);
    if (fd < 0)
        return 0;

    memset(&addr, 0, sizeof(addr));
    addr.addr.len = 4;
    addr.addr.type = ADDR_TYPE_IP;
    addr.port = 1900;
    NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(0,0,0,0);

    err = socket_bind(fd, &addr);
    printf("socket_bind returns %d\n", err);
    if (err < 0)
        return 0;

    err = socket_listen(fd);
    printf("socket_listen returns %d\n", err);
    if (err < 0)
        return 0;

    for (;;) {
        int saved_stdin, saved_stdout, saved_stderr;
        char *argv;

        new_fd = socket_accept(fd, &addr);
        printf("socket_accept returns %d\n", new_fd);
        if (new_fd < 0)
            continue;

        saved_stdin = dup(0);
        saved_stdout = dup(1);
        saved_stderr = dup(2);

        dup2(new_fd, 0);
        dup2(new_fd, 1);
        dup2(new_fd, 2);
        close(new_fd);

        // XXX launch
        argv = "/boot/bin/shell";
        err = _kern_proc_create_proc("/boot/bin/shell", "shell", &argv, 1, 5, 0);

        dup2(saved_stdin, 0);
        dup2(saved_stdout, 1);
        dup2(saved_stderr, 2);
        close(saved_stdin);
        close(saved_stdout);
        close(saved_stderr);
        printf("_kern_proc_create_proc returns %d\n", err);
    }
}

int main(int argc, char **argv)
{
//  nettest1();
//  nettest2();
//  nettest3();
//  nettest4();
//  nettest5();
    nettest6();

    return 0;
}


