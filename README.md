# Environment-Based Alarm Clock System

**Group 6 - MECH 5950 (December 2025)**

This repository contains the Arduino code for the Environment-Based Alarm Clock System.

## Required Software

Download the [Arduino IDE](https://www.arduino.cc/en/software/) to upload the code to the microcontroller.

Some required libraries are listed below. Install them via the IDE Library Manager.
- I2C communication
- LiquidCrystal I2C
- IRemote
- DHT sensor library
- Ds1302

The code used has the filename [alarm.ino](/alarm.ino).

## Hardware

This project was made using the **Keyes Ultimate Starter Kit**. Any Arduino starter kit that has the required components should work just as well, though this ultimately depends on the specific components in question.

The components used are:

- Arduino Uno microcontroller
- Temperature and humidity sensor
- Real-Time Clock (RTC) module
- Infrared (IR) receiver
- IR transmitter
- Liquid crystal display (LCD) monitor
- Remote IR controller
- Buzzer module
- Breadboard
- Wires
- LED bulb
- Battery

The schematic of the device is as shown:

![Schematic](/schematic.jpeg)

## Usage

Note that this system only works with air conditioner units that communicate using IR. Future iterations of this device could incorporate support for other communication protocols.

Once the device is assembled correctly, Arduino Uno microcontroller is connected to the PC and the Arduino IDE is configured to communicate with the board, simply upload the code to the board.

Once it is powered on and the air conditioner remote IR codes are successfully captured, the system will keep track of time via the RTC module. By default, the alarm time is 6:30 am. To change the time, press the OK button and use the UP and DOWN keys to change the hour, and LEFT and RIGHT keys to change the minute. Pressing OK again will save this new time.

5 minutes before the set time, the IR transmitter will send a signal to the air conditioner unit to power up, and starts a 5-minute timer until the alarm time.

At the time of the alarm, the room will be uncomfortably hot/cold depending on the setting used (this may vary with the size of the room), and the secondary alarm (the buzzer module) will activate, and the system randomly generates a 4-digit code. To disable the alarm, press the 4-digit code shown and then the OK button. Note that the alarm will continue until the correct code is pressed.

After the alarm is turned off, it will activate again the next time the given alarm time is reached.

## License

This project is licensed under the MIT License.
