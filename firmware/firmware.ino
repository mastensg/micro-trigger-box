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
#include <SPI.h>
#include <Ethernet.h>
#include <utility/w5100.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define CAMERA 8
#define LED 9

SOCKET s;
byte rbuf[1500 + 14];
int rbuflen;
uint8_t calibrated = 0;
uint8_t led_state = 0;
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

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

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
    int16_t ret;
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

    led_state = !led_state;
    digitalWrite(LED, led_state);
}
