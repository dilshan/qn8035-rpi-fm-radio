# QN8035 based Raspberry Pi FM radio receiver.

[QN8035](https://datasheetspdf.com/pdf-down/Q/N/8/QN8035_Quintic.pdf) is a single-chip stereo FM radio receiver IC designed by the *Quintic Corporation*. This receiver supports the FM broadcast band ranging from 60MHz to 108MHz. 

This repository contains a *[Raspberry Pi](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/)* base QN8035 receiver to receive FM stereo broadcasts from 88MHz to 108MHz.

[![Video preview](https://github.com/dilshan/qn8035-rpi-fm-radio/blob/main/resources/qn8035-video-preview.jpg)](https://www.youtube.com/watch?v=qJ5dKJD2EvY)

The hardware part of this receiver consists of a QN8035 tuner and a 32.768kHz oscillator circuit. These circuits are designed to work with a 3.3V power supply.

The prototype version of this tuner designs with a single-sided PCB. All the schematics and *KiCAD* PCB design files are available in this repository. The Gerber files of this PCB are available to download at the release section of this repository.

![QN8035 and Raspberry Pi3 connection diagram](https://raw.githubusercontent.com/dilshan/qn8035-rpi-fm-radio/main/resources/qn8035-rpi3-connection.jpg)

The QN8035 uses the I2C bus to communicate with the host system. The tuner application develops with this system utilizes I2C channel 1 (*Raspberry Pi* header Pin 3 and 5) to communicate with the QN8035. The control software presents in this repository is designed to work with the *Raspberry Pi* operating system and is set to run on May 7th, 2021 (*Buster*), or the latest releases.

The tuner application provided in this release supports the following features:

 - Manual and automatic station scanning. 
 - Decode RDS PS (program service) data. 
 - Volume control.
 - Display RSSI and SNR readings receive from the tuner.

The control application of this receiver develops using *GCC* and *[WiringPi](http://wiringpi.com/)* library. 

The complete construction and configuration steps of this tuner show in the video above.

This is an open-source hardware project. All schematics, design files, and PCB layouts have been released under the [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/) license. The control software is released under the terms of the [MIT License](https://opensource.org/licenses/MIT).

