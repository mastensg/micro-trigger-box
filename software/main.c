/*
 * Copyright (c) 2013 Simula Research Laboratory AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Simula Research Laboratory AS.
 * 4. Neither the name of Simula Research Laboratory AS nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Simula Research Laboratory AS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Simula Research Laboratory AS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <netpacket/packet.h>
#include <net/if.h> 
#include <netinet/ether.h>

struct 
{
    int fd;
    struct sockaddr_ll address;
} sock;

static void
realtime_init()
{
    struct sched_param sp;

    if (geteuid())
        errx(EXIT_FAILURE, "not root");

    if (-1 == (sp.sched_priority = sched_get_priority_max(SCHED_FIFO)))
        err(EXIT_FAILURE, "sched_get_priority_max");

    if (sched_setscheduler(0, SCHED_FIFO, &sp))
        err(EXIT_FAILURE, "sched_setscheduler");

    if (-1 == mlockall(MCL_CURRENT | MCL_FUTURE))
        err(EXIT_FAILURE, "mlockall");
}

static void
ethernet_init(const char *interface)
{
    struct ifreq if_idx;

    if (-1 == (sock.fd = socket(AF_PACKET, SOCK_DGRAM, 0)))
        err(EXIT_FAILURE, "socket");

    memset(&if_idx, 0, sizeof(if_idx));
    strncpy(if_idx.ifr_name, interface, IFNAMSIZ);

    if (-1 == ioctl(sock.fd, SIOCGIFINDEX, &if_idx))
        err(EXIT_FAILURE, "SIOCGIFINDEX");

    sock.address.sll_ifindex = if_idx.ifr_ifindex;
    sock.address.sll_halen = ETH_ALEN;
    sock.address.sll_protocol = htons(0x4d53);
    sock.address.sll_addr[0] = 0xff;
    sock.address.sll_addr[1] = 0xff;
    sock.address.sll_addr[2] = 0xff;
    sock.address.sll_addr[3] = 0xff;
    sock.address.sll_addr[4] = 0xff;
    sock.address.sll_addr[5] = 0xff;
}

static void
ethernet_broadcast(uint8_t framerate)
{
    ssize_t numbytes;

#ifndef NDEBUG
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(stderr, "0.%09ld\n", ts.tv_nsec);
#endif

    numbytes = sendto(sock.fd, &framerate, sizeof(framerate), 0,
            (struct sockaddr *)&sock.address,
            sizeof(sock.address));

    if (-1 == numbytes)
        warn("sendto");
}

static void
next_second()
{
    const long one_second = 1000000000;
    const long time_to_spin = 10000000;
    struct timespec time_of_call, length_of_sleep, time_now;

    if (-1 == clock_gettime(CLOCK_REALTIME, &time_of_call))
        err(EXIT_FAILURE, "clock_gettime");

    length_of_sleep.tv_sec = 0;
    length_of_sleep.tv_nsec = one_second - time_to_spin - time_of_call.tv_nsec;

    assert(one_second > length_of_sleep.tv_nsec);

    if (0 < length_of_sleep.tv_nsec)
    {
        if (-1 == nanosleep(&length_of_sleep, NULL))
            warn("nanosleep");
    }

    do
    {
        if (-1 == clock_gettime(CLOCK_REALTIME, &time_now))
            err(EXIT_FAILURE, "clock_gettime");
    }
    while (time_now.tv_sec == time_of_call.tv_sec);
}

int
main(int argc, char *argv[])
{
    long framerate;
    char *end, *interface, usage[256];

    sprintf(usage, "\nUsage: %s interface framerate\n\n"
            "For example: %s eth0 30", argv[0], argv[0]);

    if (3 != argc)
        errx(EXIT_FAILURE, "Wrong number of arguments\n%s", usage);

    interface = argv[1];

    if (IFNAMSIZ == strnlen(interface, IFNAMSIZ))
        errx(EXIT_FAILURE, "Too long interface name\n%s", usage);

    if (4 == strnlen(argv[2], 4))
        errx(EXIT_FAILURE, "Too long framerate number\n%s", usage);

    framerate = strtol(argv[2], &end, 10);

    if (*end || framerate < 1 || framerate > 255)
        errx(EXIT_FAILURE, "Framerate must be [1 - 255]\n%s", usage);

    realtime_init();

    ethernet_init(interface);

    for (;;)
    {
        next_second();
        ethernet_broadcast(framerate);
    }

    return EXIT_SUCCESS;
}
