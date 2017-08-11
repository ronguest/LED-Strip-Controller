LED Strip Controller
========

* Uses a Adafruit Feather Huzzah to control a strip of Neopixels
* Interfaces with Adafruit IO using MQTT to accept input/control
* Uses the EEPROM to store a color
* Changes color at surrogate sunrise and sunset times (fixed, not actual)
* Turns off all the LEDS at 'bed time'
* Fetches time using NTP and uses the local time Zone
