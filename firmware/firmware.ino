#include <SPI.h>
#include <Ethernet.h>
#include <utility/w5100.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define CAMERA 8

SOCKET s;
byte rbuf[1500 + 14];
int rbuflen;
uint8_t calibrated = 0;
uint32_t second;
uint32_t fps;

static int16_t
recv_fps()
{
    W5100.recv_data_processing(s, rbuf, rbuflen);
    W5100.execCmdSn(s, Sock_RECV);

    if (62 != rbuflen)
        return -1;

    if (0x4d != rbuf[14])
        return -1;

    if (0x53 != rbuf[15])
        return -1;

    return rbuf[16];
}

static void
trigger()
{
    digitalWrite(CAMERA, HIGH);
    delayMicroseconds(50);
    digitalWrite(CAMERA, LOW);
}

void
setup()
{
    pinMode(CAMERA, OUTPUT);
    digitalWrite(CAMERA, LOW);

    W5100.init();
    W5100.writeSnMR(s, SnMR::MACRAW); 
    W5100.execCmdSn(s, Sock_OPEN);

    TCCR1A = 0;

    /* Divide clock by 256.
     * 16 MHz / 256 = 62500 Hz
     * 1 / 62500 Hz = 16 µs
     */

    TCCR1B = 1 << CS12;
}

void
loop()
{
    int8_t ret;
    uint32_t i, t;

    /* Wait for once-a-second packet. */
    for (;;)
    {
        while (0 == (rbuflen = W5100.getRXReceivedSize(s)))
            ;

        t = TCNT1;
        TCNT1 = 0;

        if (-1 == (ret = recv_fps()))
            continue;

        fps = ret;

        if (calibrated)
        {
            if (t > second)
                ++second;
            else if (t < second)
                --second;
        }
        else
        {
            second = t;
        }

        if (0 == (rbuflen = W5100.getRXReceivedSize(s)))
            break;
    }

    if (second < 62000 || 63000 < second)
        return;

    calibrated = 1;

    /* Trigger camera. */
    for (i = 0; i < fps; ++i)
    {
        while (TCNT1 < i * second / fps)
            ;

        trigger();
    }
}
