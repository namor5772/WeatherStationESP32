# WeatherStationESP32

Weather Station based on a ESP32 board using MQTT.

This document describes the design, building and programming of an IOT enabled weather station. At regular intervals (eg. every 15 minutes) it measures a range of weather and other statistics, saving them as time stamped rows to an updated csv file. In particular:

1. Temperature (C)
1. Pressure (HPa)
1. Humidity (%)
1. Rainfall (mm/h)
1. Wind speed (km/h)
1. Wind direction (deg)
1. RTC temp (C)
1. Current (A) - being drawn from the solar panel charged battery
1. Voltage (V) - at the inputs to the circuits voltage regulator
1. Power (W) - drawn by the weather station ( as A*V above)

This data can be remotely accessed via MQTT, which also enables add-hoc contol and data requests. Bluetooth control is also implemented.

The hardware is based around an ESP32 module on a small Vero strip board that contains the Voltage Regulator and all other circuits. Everything is powered by a 12V 100Ah AGM Deep Cycle Battery which is charged by a 12V 150W Solar panel via a PWM Solar charge controller. The 12V input is regulated to 5.2V. This is used as required for charging the wifi modem, powering the ESP32 module, servos and the digital relay. The BME280 and INA260 I2C 3.3V sensors are powered via the ESP32.
