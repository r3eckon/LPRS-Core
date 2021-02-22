# LPRS-Core
Gerbers and Code for Industrial Low Power Remote Sensors

## What is it?
WeAct STM32 "Blackpill" & REYAX RYLR896 Lora Module socket PCBs for modular low power industrial remote sensors. 
The board design allows each component to be swapped when failing or reused when no longer needed.

Remote information is transmitted to a Lora module attached to a Raspberry Pi.
Code base contains a simple C language console debugging program and a GUI program.
GUI has industrial monitoring style display using gauges, water level indicators, graphs.

## Current Board Types
* Basic Radio Transceiver
* Ultrasonic Distance Sensor
* Differential Pressure Sensor

![Current Board Types](https://i.imgur.com/noyWctf.png)

## Monitoring GUI & Debugger Program
![Monitoring GUI Test](https://i.imgur.com/C9BgKUJ.png)

https://www.youtube.com/watch?v=clqiaxmq7VI

The debugger program must be running on the Pi to received Lora data and store it into a data file.
The GUI program will read this data file at a regular interval and update gauges accordingly.

## STM32 MCU Code
Each board version has a different program running on the microcontroller. 
As I'm not experienced with STM Cube IDE the Arduino IDE has been used.
Blackpills come with USB Bootloader preinstalled so no special setup is required.
Arduino IDE must be configured to work with board, various tutorials are on YouTube.

Board to Board configuration involves setting the various #define at the top of the file to correct address and sensor ID.
