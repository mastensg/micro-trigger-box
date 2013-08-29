Micro Trigger Box
=================

Firmware
--------

Connect a 5V FTDI cable to the 1x6-pin female header on the Arduino Ethernet
board. Then

    sudo apt-get install arduino-core arduino-mk make
    sudo addgroup `whoami` dialout
    newgrp dialout
    cd firmware
    make upload

or upload the firmware with the Arduino IDE.

Software
--------

    sudo apt-get install dpkg-dev
    cd software
    autoreconf -f -i
    dpkg-buildpackage
    sudo dpkg -i ../trigger_VERSION_ARCHITECTURE.deb

or

    cd software
    autoreconf -f -i
    ./configure
    make
    sudo make install
