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

const uint32_t second = 1000000000;

struct 
{
    int fd;
    struct sockaddr_ll address;
} sock;

static void
realtime()
{
    struct sched_param sp;

    if (geteuid())
        errx(EXIT_FAILURE, "not root");

    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);

    if (sched_setscheduler(0, SCHED_FIFO, &sp))
        err(EXIT_FAILURE, "sched_setscheduler");

    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        err(EXIT_FAILURE, "mlockall");
}

static void
ethernet_init(const char *interface)
{
    struct ifreq if_idx;

    if (-1 == (sock.fd = socket(AF_PACKET, SOCK_DGRAM, 0)))
        err(EXIT_FAILURE, "socket");

    // TODO: rewrite this block.
    memset(&if_idx, 0, sizeof(if_idx));
    strncpy(if_idx.ifr_name, interface, 7);
    if (ioctl(sock.fd, SIOCGIFINDEX, &if_idx) < 0)
        perror("SIOCGIFINDEX");

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
trigger()
{
    ssize_t numbytes;
    // TODO: arbitrary framerate
    static const uint8_t packet[1] = {50};

#ifdef DEBUG
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(stderr, "0.%09ld\n", ts.tv_nsec);
#endif

    numbytes = sendto(sock.fd, packet, sizeof(packet), 0,
            (struct sockaddr *)&sock.address,
            sizeof(sock.address));

    if (-1 == numbytes)
        err(EXIT_FAILURE, "sendto");
}

static void
sleep_until(long then)
{
    static const uint32_t spinns = 10000000;
    struct timespec tq;
    int32_t sleepns;
    long now;

    clock_gettime(CLOCK_REALTIME, &tq);
    now = tq.tv_nsec;

    if (then < now)
        then += second;

    sleepns = then - spinns - now;
    if (sleepns > 0)
        usleep(sleepns / 1000);
}

static void
spin_until(long then)
{
    struct timespec tq;

    do
    {
        clock_gettime(CLOCK_REALTIME, &tq);
    }
    while (tq.tv_nsec < then);
}

static void
next_second()
{
    struct timespec tq, tp;

    sleep_until(0);

    clock_gettime(CLOCK_REALTIME, &tq);
    do
    {
        clock_gettime(CLOCK_REALTIME, &tp);
    }
    while (tp.tv_nsec > tq.tv_nsec);
}

static void
until(long then)
{
    struct timespec tq;

    clock_gettime(CLOCK_REALTIME, &tq);
    if (tq.tv_nsec > then)
        next_second();

    sleep_until(then);
    spin_until(then);
}

static void
loop(int frequency, void (*handler)(void))
{
    uint32_t period;
    int i;

    period = second / frequency;

    for (;;)
    {
        next_second();
        handler();

        for (i = 1; i < frequency; ++i)
        {
            until(i * period);
            //spin_until(i * period);
            handler();
        }
    }
}

int
main(int argc, char *argv[])
{
    realtime();
    ethernet_init("eth1");
    loop(1, trigger);

    return EXIT_SUCCESS;
}
