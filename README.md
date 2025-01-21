### What is this for
This project is proof of concept for wireless power switch built on esp8266 platform. Why? Because my subwoofer amplifier is far from my desk and I am too lazy to walk there to turn it on. :)

### Features
Automatically connects to the wifi network defined in wifi_credentials.h.
You can access the (very simple) GUI via HTTP from any device in your local network.
You can also send raw TCP packet with either 0 (off) or 1 (on) to control the relay.

### Building and uploading
Make sure you have Espressif 8266 platform installed on platformio.
Connect the board via usb and run:
`pio run -t uploadfs`
