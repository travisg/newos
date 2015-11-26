#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct blockdevice {
    int id;
    int filefd;
    off_t size;
} blockdevice;

static blockdevice *dev;
static int sockfd;

#define BLOCKSIZE 512

#define MAGIC 0x14983543

typedef struct message {
    int magic;
    int block_id;
    int command;
    int arg;
    int arg2;
    char data[0];
} message;

#define CMD_NULL 0
#define CMD_GETSIZE 1
#define CMD_READBLOCK 2
#define CMD_WRITEBLOCK 3

static int validate_message(message *msg, int len)
{
    if (len < sizeof(message))
        return 0;

    // validate the message
    if (ntohl(msg->magic) != MAGIC)
        return 0;
    if (ntohl(msg->block_id) != 0)
        return 0;

    switch (ntohl(msg->command)) {
        case CMD_NULL:
        case ~CMD_NULL:
        case CMD_GETSIZE:
        case ~CMD_GETSIZE:
        case CMD_READBLOCK:
        case ~CMD_READBLOCK:
        case CMD_WRITEBLOCK:
        case ~CMD_WRITEBLOCK:
            break;
        default:
            return 0;
    }

    return 1;
}

static int net_loop()
{
    struct sockaddr_in from_addr;
    socklen_t slen = sizeof(struct sockaddr);
    char buf[4096];
    int err;
    int reply_size;

    for (;;) {
        err = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&from_addr, &slen);
        if (err < 0) {
            printf("error reading from socket\n");
            return -1;
        }

        message *msg = (message *)buf;

        if (!validate_message(msg, err))
            continue;

//      printf("got message, command %d\n", ntohl(msg->command));
        reply_size = sizeof(message);
        switch (ntohl(msg->command)) {
            case CMD_NULL:
                // respond
                msg->command = htonl(~CMD_NULL);
                break;
            case CMD_GETSIZE:
                msg->command = htonl(~CMD_GETSIZE);
                printf("GETSIZE %d\n", htonl(dev->size));
                *(int *)&msg->data[0] = htonl(dev->size);
                reply_size += 4;
                break;
            case CMD_READBLOCK:
                msg->command = htonl(~CMD_READBLOCK);
                printf("R %d %d\n", ntohl(msg->arg), ntohl(msg->arg2));
                if (ntohl(msg->arg) + ntohl(msg->arg2) <= dev->size) {
                    lseek(dev->filefd, ntohl(msg->arg), SEEK_SET);
                    read(dev->filefd, &msg->data[0], ntohl(msg->arg2));
                } else {
                    printf("CMD_READBLOCK: invalid offset %d and size %d\n", ntohl(msg->arg), ntohl(msg->arg2));
                    memset(&msg->data[0], 0, ntohl(msg->arg2));
                }
                reply_size += BLOCKSIZE;
                break;
            case CMD_WRITEBLOCK:
                msg->command = htonl(~CMD_WRITEBLOCK);
                printf("W %d %d\n", ntohl(msg->arg), ntohl(msg->arg2));
                if (ntohl(msg->arg) + ntohl(msg->arg2) <= dev->size) {
                    lseek(dev->filefd, ntohl(msg->arg), SEEK_SET);
                    write(dev->filefd, &msg->data[0], ntohl(msg->arg2));
                } else {
                    printf("CMD_WRITEBLOCK: invalid offset %d and size %d\n", ntohl(msg->arg), ntohl(msg->arg2));
                }
                break;
            default:
                continue;
        }

        // send the reply
        err = sendto(sockfd, buf, reply_size, 0, (struct sockaddr *)&from_addr, slen);
        if (err < 0) {
            printf("error replying to message\n");
            return -1;
        }
    }

    return 0;
}

static void usage()
{
    printf("usage: netblock <blockdev file>\n");
}

int main(int argc, char *argv[])
{
    int err;

    if (argc < 2) {
        usage();
        return -1;
    }

    dev = NULL;
    sockfd = -1;

    dev = new blockdevice;

    // open the block device and figure out how big it is
    dev->filefd = open(argv[1], O_RDWR|O_BINARY);
    if (dev->filefd < 0) {
        printf("error opening file '%s'\n", argv[1]);
        return -1;
    }

    struct stat stat;
    err = fstat(dev->filefd, &stat);
    if (err < 0) {
        printf("error statting file\n");
        return -1;
    }

    if (stat.st_size < 512) {
        printf("file is too small to use as a block device\n");
        return -1;
    }

    dev->size = stat.st_size;

    printf("opened file '%s', size %Ld\n", argv[1], dev->size);

    dev->id = 0;

    // open the server socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    memset(&addr.sin_addr, 0, sizeof(addr.sin_addr));

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("error creating socket\n");
        return -1;
    }

    err = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
    if (err < 0) {
        printf("error binding socket\n");
        return -1;
    }

    net_loop();

    return 0;
}
