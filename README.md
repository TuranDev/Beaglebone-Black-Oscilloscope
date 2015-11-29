# Beaglebone-Black-Oscilloscope

This is the instructions and code for a little side project I undertook while doing some DSP work at university.
With a few components (Listed Below) It turns a Beaglebone Black into a simple oscilliscope annd signal generator.

![PIcture](https://github.com/TuranSuleyman/Beaglebone-Black-Oscilloscope/blob/master/images/IMG_20140424_153454.jpg "Oscilliscope")

Components I used to build:

* 1x BeagleBone Black
* 1x Beaglebone Audio Cape
* 1x Graphic ST7565 Positive LCD Display - https://www.adafruit.com/products/250
* 1x 330Ω Resistor
* 4x Push Switches
* 4x 1k Resistors
* 5x 10k Potentiometers 
 
Input Functions

| Input        | Menu           | Oscilloscope  | Signal Generator |
|--------------|----------------|---------------|------------------| 
| Select     | Scroll Modes |  | |
| Start      | Enter Mode     |   Back To Menu | Back To Menu |
| Button 1 |      |    Offset + | Channel 1 Wave Type |
| Button 2 |       |    Offset - | Channel 2 Wave Type |
| AIN0 |    |     | Channel 2 Frequency| 
| AIN1 |     |    | Channel 1 Frequency |
| AIN4 |       |    Trigger Level Adjust |
| AIN5 |       |    Voltage Division | Channel 2 Amplitude|
| AIN6 |       |    Time Division |Channel 1 Amplitude|
