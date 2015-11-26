/*
** Copyright 2003-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/syscalls.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <newos/errors.h>
#include <newos/drivers.h>

#include "tests.h"

namespace {
struct testcdtors {
    testcdtors() {
        printf("(testing constructor)\n");
    }
    ~testcdtors() {
        printf("(testing destructor)\n");
    }
};
testcdtors gtest;
}

struct test_option {
    const char *command;
    const char *name;
    int (*func)(int arg);
    int arg;
};

static test_option opts[] = {
    { "0", "sleep test", &sleep_test, 0 },
    { "1", "spawn threads, run forever", &thread_spawn_test, 0 },
    { "2", "spawn threads, kill them", &thread_spawn_test, 1 },
    { "3", "spawn threads, self terminating", &thread_spawn_test, 2 },
    { "4", "syscall benchmark", &syscall_bench, 0 },
    { "5", "test signals", &sig_test, 0 },
    { "6", "fpu safety test", &fpu_test, 0 },
    { 0, 0, 0, 0 }
};

static int get_line(char *buf, int buflen)
{
    int i = 0;
    char c;

    for (;;) {
        c = getchar();
        if (c == '\n' || c == '\r') {
            buf[i] = 0;
            break;
        }

        buf[i++] = tolower(c);
        if (i == buflen - 1) {
            buf[i] = 0;
            break;
        }
    }
    return i;
}

int main(int argc, char **argv)
{
    char command[128];

    printf("welcome to the newos testapp!\n");

    for (int cnt = 0; cnt< argc; cnt++) {
        printf("arg %d = %s \n",cnt,argv[cnt]);
    }

retry:
    printf("Select from tests below, or 'x' to exit:\n");

    for (test_option *opt = opts; opt->command; opt++) {
        printf("%s\t%s\n", opt->command, opt->name);
    }
    printf("test# ");

retry_line:
    get_line(command, sizeof(command));

    if (strcmp(command, "x") == 0)
        return 0;
    if (strlen(command) == 0)
        goto retry_line;

    for (test_option *opt = opts; opt->command; opt++) {
        if (strcmp(command, opt->command) == 0) {
            printf("\n");
            int err = opt->func(opt->arg);
            printf("test returns %d\n", err);
            break;
        }
    }

    goto retry;
}

int foo(int argc, char **argv)
{
    int rc = 0;
#if 0
    int fd;
    size_t len;
    char c;
    int cnt;
#endif

#if 0
    fd = open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
    if (fd >= 0) {
        for (;;) {
            size_t len;
            static char buf[] = {
                0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
                0x00, 0x08, 0x00, 0x45, 0x00, 0x00, 0x28, 0xF9, 0x55, 0x40, 0x00,
                0x40, 0x06, 0xC0, 0x02, 0xC0, 0xA8, 0x00, 0x01, 0xC0, 0xA8, 0x00,
                0x26, 0x00, 0x50, 0x0B, 0x5C, 0x81, 0xD6, 0xFA, 0x48, 0xBB, 0x17,
                0x03, 0xC9, 0x50, 0x10, 0x7B, 0xB0, 0x6C, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };
            len = sizeof(buf);
            pwrite(fd, buf, &len, 0);
        }
    }
#endif
#if 0
    fd = open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
    if (fd >= 0) {
        int foo = 0;
        for (;;) {
            size_t len;
            char buf[1500];

            len = sizeof(buf);
            pread(fd, buf, &len, 0);
            printf("%d read %d bytes\n", foo++, len);
        }
    }
#endif

// XXX dangerous! This overwrites the beginning of your hard drive
#if 0
    {
        char buf[512];
        size_t bytes_read;

        printf("opening /dev/bus/ide/0/0/raw\n");
        fd = open("/dev/bus/ide/0/0/raw", "", STREAM_TYPE_DEVICE);
        printf("fd = %d\n", fd);

        bytes_read = 512;
        rc = pread(fd, buf, &bytes_read, 0);
        printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

        buf[0] = 'f';
        buf[1] = 'o';
        buf[2] = 'o';
        buf[3] = '2';
        buf[4] = 0;

        bytes_read = 512;
        rc = pwrite(fd, buf, &bytes_read, 1024);
        printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

        close(fd);
    }
#endif
#if 0
    {
        thread_id tids[10];
        int i;

        for (i=0; i<10; i++) {
            tids[i] = _kern_thread_create_thread("foo", &test_thread, (void *)i);
            _kern_thread_resume_thread(tids[i]);
        }

        usleep(5000000);
        _kern_proc_kill_proc(_kern_get_current_proc_id());
        /*
                usleep(3000000);
                for(i=0; i<10; i++) {
                    _kern_thread_kill_thread(tids[i]);
                }
                printf("thread_is dead\n");
                usleep(5000000);
        */
    }
#endif
#if 0
    {
        for (;;)
            _kern_proc_create_proc("/boot/bin/true", "true", 32);
    }
#endif
#if 0
    {
        void *buf = (void *)0x60000000;
        int fd;
        int rc;
        int len = 512;

        fd = open("/boot/testapp", "", STREAM_TYPE_FILE);

        rc = pread(fd, buf, &len, 0);
        printf("rc from read = 0x%x\n", rc);
        close(fd);
    }
#endif
#if 0
    {
        char data;
        int fd;

        fd = open("/dev/audio/pcbeep/1", STREAM_TYPE_DEVICE, 0);
        if (fd >= 0) {
            printf("writing to the speaker\n");
            data = 3;
            pwrite(fd, &data, 1, 0);
            usleep(1000000);
            data = 0;
            pwrite(fd, &data, 1, 0);
            close(fd);
        }
    }
#endif

#if 0
    {
        port_test();
    }
#endif
#if 0
    {
        int fd, bytes_read;
        char buf[3];

        fd = open("/dev/ps2mouse", STREAM_TYPE_DEVICE, 0);
        if (fd < 0) {
            printf("failed to open device\n");
            return -1;
        }

        bytes_read = read(fd, buf, 3);
        if (bytes_read < 3) {
            printf("failed to read device\n");
            return -1;
        }

        printf("Status: %X\nDelta X: %d\nDelta Y: %d\n", buf[0], buf[1], buf[2]);
    }
#endif
#if 0
    {
        int fd;
        int buf[512/4];
        int i, j;

        fd = open("/dev/disk/netblock/0/raw", STREAM_TYPE_DEVICE, 0);
        if (fd < 0) {
            printf("could not open netblock\n");
            return -1;
        }

        ioctl(fd, 90001, NULL, 0);

        for (i=0; i<1*1024*1024; i += 512) {
            for (j=0; j<512/4; j++)
                buf[j] = i + j*4;
            write(fd, buf, sizeof(buf));
        }

//      pread(fd, buf, sizeof(buf), 0);
//      pwrite(fd, buf, sizeof(buf), 512);
    }
#endif
#if 0

#define NUM_FDS 150
    {
        int             fds[NUM_FDS], i;
        struct rlimit   rl;

        rc = getrlimit(RLIMIT_NOFILE, &rl);
        if (rc < 0) {
            printf("error in getrlimit\n");
            return -1;
        }

        printf("RLIMIT_NOFILE = %lu\n", rl.rlim_cur);

        for (i = 0; i < NUM_FDS; i++) {
            fds[i] = open("/boot/bin/testapp", 0);
            if (fds[i] < 0) {
                break;
            }
        }

        printf("opened %d files\n", i);

        rl.rlim_cur += 15;

        printf("Setting RLIMIT_NOFILE to %lu\n", rl.rlim_cur);
        rc = setrlimit(RLIMIT_NOFILE, &rl);
        if (rc < 0) {
            printf("error in setrlimit\n");
            return -1;
        }

        rl.rlim_cur = 0;
        rc = getrlimit(RLIMIT_NOFILE, &rl);
        if (rc < 0) {
            printf("error in getrlimit\n");
            return -1;
        }

        printf("RLIMIT_NOFILE = %lu\n", rl.rlim_cur);

        for (; i < NUM_FDS; i++) {
            fds[i] = open("/boot/bin/testapp", 0);
            if (fds[i] < 0) {
                break;
            }
        }

        printf("opened a total of %d files\n", i);
    }
#endif
#if 0
    {
        region_id rid;
        void *ptr;

        rid = _kern_vm_map_file("netblock", &ptr, REGION_ADDR_ANY_ADDRESS, 16*1024, LOCK_RW,
                                REGION_NO_PRIVATE_MAP, "/dev/disk/netblock/0/raw", 0);
        if (rid < 0) {
            printf("error mmaping device\n");
            return -1;
        }

        // play around with it
        printf("mmaped device at %p\n", ptr);
        printf("%d\n", *(int *)ptr);
        printf("%d\n", *((int *)ptr + 1));
        printf("%d\n", *((int *)ptr + 2));
        printf("%d\n", *((int *)ptr + 3));
    }
#endif
#if 0
    {
        int i;

//      printf("spawning %d copies of true\n", 10000);

        for (i=0; ; i++) {
            proc_id id;
            bigtime_t t;

            printf("%d...", i);

            t = _kern_system_time();

            id = _kern_proc_create_proc("/boot/bin/true", "true", NULL, 0, 20);
            if (id <= 0x2) {
                printf("new proc returned 0x%x!\n", id);
                return -1;
            }
            _kern_proc_wait_on_proc(id, NULL);

            printf("done (%Ld usecs)\n", _kern_system_time() - t);
        }
    }
#endif
#if 0
    {
        int i;

        printf("spawning two cpu eaters\n");

//      _kern_thread_resume_thread(_kern_thread_create_thread("cpu eater 1", &cpu_eater_thread, 0));
//      _kern_thread_resume_thread(_kern_thread_create_thread("cpu eater 2", &cpu_eater_thread, 0));

        printf("spawning %d threads\n", 10000);

        for (i=0; i<10000; i++) {
            thread_id id;
            bigtime_t t;

            printf("%d...", i);

            t = _kern_system_time();

            id = _kern_thread_create_thread("testthread", &dummy_thread, 0);
            _kern_thread_resume_thread(id);
            _kern_thread_wait_on_thread(id, NULL);

            printf("done (%Ld usecs)\n", _kern_system_time() - t);
        }
    }
#endif
#if 0
    {
        thread_id id;
        static double f[5] = { 2.43, 5.23, 342.34, 234123.2, 1.4 };

        printf("spawning a few floating point crunchers\n");

        id = _kern_thread_create_thread("fpu thread0", &fpu_cruncher_thread, &f[0]);
        _kern_thread_resume_thread(id);

        id = _kern_thread_create_thread("fpu thread1", &fpu_cruncher_thread, &f[1]);
        _kern_thread_resume_thread(id);

        id = _kern_thread_create_thread("fpu thread2", &fpu_cruncher_thread, &f[2]);
        _kern_thread_resume_thread(id);

        id = _kern_thread_create_thread("fpu thread3", &fpu_cruncher_thread, &f[3]);
        _kern_thread_resume_thread(id);

        id = _kern_thread_create_thread("fpu thread4", &fpu_cruncher_thread, &f[4]);
        _kern_thread_resume_thread(id);

        getchar();
        printf("passed the test\n");
    }
#endif
#if 0
    rc = pipe_test();
#endif
#if 0
    {
        int i,j,k;
        int fd;
        devfs_framebuffer_info fb;
        int err;
        void *framebuffer;

        fd = open("/dev/graphics/fb/0", 0);
        if (fd < 0) {
            printf("error opening framebuffer device\n");
            return -1;
        }

        err = ioctl(fd, IOCTL_DEVFS_GET_FRAMEBUFFER_INFO, &fb, sizeof(fb));
        if (err < 0) {
            printf("error getting framebuffer info\n");
            return -1;
        }

        err = ioctl(fd, IOCTL_DEVFS_MAP_FRAMEBUFFER, &framebuffer, sizeof(framebuffer));
        if (err < 0) {
            printf("error mapping framebuffer\n");
            return -1;
        }

        for (k=0;; k++) {
            for (i=0; i<fb.height; i++) {
                uint16 row[fb.width];
                for (j=0; j<fb.width; j++) {
                    uint16 color = ((j+i+k) & 0x1f) << 11;
                    row[j] = color;
                }
                memcpy(framebuffer + i*fb.width*2, row, sizeof(row));
            }
        }
    }
#endif
#if 0
    {
        int err;

        printf("mounting nfs filesystem\n");

        _kern_mkdir("/nfs");
        err = _kern_mount("/nfs", "192.168.0.4:/disk", "nfs", NULL);
        printf("mount returns %d\n", err);

        {
            int i;
            int fd;
            char buf[1024];

            fd = _kern_opendir("/nfs");

            for (i=0; i<16; i++) {
                memset(buf, 0, sizeof(buf));
                err = _kern_readdir(fd, buf, sizeof(buf));
                printf("read returns %d '%s'\n", err, buf);
            }
        }
    }
#endif
    printf("exiting w/return code %d\n", rc);
    return rc;
}


