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
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>

#include "netboot.h"

int main(int argc, char *argv[])
{
    struct sockaddr_in dst,src;
    char buf[128];
    net_boot nb,nbr;
    int fd;
    struct timeval timeout;
    int s,l,i;
    fd_set read_fds;
    
    if(argc != 3) {
        fprintf(stderr,"usage: test <filename> <addr>\n");
        exit(1);
        
    }        
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = inet_addr(argv[2]);
    dst.sin_port = htons(NETBOOT_PORT);

    src.sin_family = AF_INET;
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    src.sin_port = htons(NETBOOT_PORT);
    
    if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        perror("socket");

    if(bind(s, (struct sockaddr *) &src, sizeof(src)) == -1)
        perror("bind");

    fprintf(stderr,"loading [");
    
    fd = open(argv[1],O_RDONLY);

    i = 0;
    
    while(read(fd,&(nb.data),1024) > 0){
        nb.cmd = htons(NETBOOT_CMD_LOAD);
        nb.blk = htons(i);

      retry:
        if(sendto(s, &nb, sizeof(nb), 0, (struct sockaddr *) &dst, sizeof(dst))
           == -1){
            perror("sendto");
        }
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&read_fds);
        FD_SET(s,&read_fds);
        
        if(select(s+1, &read_fds, NULL, NULL, &timeout) == 1){
            
            if(recvfrom(s,&nbr,sizeof(nbr),0,NULL,NULL) < 0) {
                fprintf(stderr,"e");
                goto retry;
            }
            if(nbr.cmd != htons(NETBOOT_CMD_ACK)) {
                fprintf(stderr,"e");
                goto retry;
            }
            if(nbr.blk != nb.blk) {
                fprintf(stderr,"e");
                goto retry;
            }
            fprintf(stderr,".");
        } else {
            fprintf(stderr,"T");
            goto retry;
        }
        i++;
    }

    nb.cmd = htons(NETBOOT_CMD_EXEC);
    nb.blk = htons(0);
    
    if(sendto(s, &nb, 4, 0, (struct sockaddr *) &dst, sizeof(dst))
       == -1){
        perror("sendto");
    }
    
/*    if(recvfrom(s,&nb,sizeof(nb),0,NULL,NULL) < 0) perror("recvfrom");
    if(nb.cmd == htons(NETBOOT_CMD_ACK)) fprintf(stderr,"!");*/
    
    fprintf(stderr,"] done\n");
    
    close(s);
    
    return 0;
    
}

