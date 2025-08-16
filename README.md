# AXA Remote component for ESPHome.

[![GitHub Release][releases-shield]][releases]
[![Licence][license-shield]][license]
[![Maintainer][maintainer-shield]][maintainer]  
[![GitHub Sponsors][github-shield]][github]
[![PayPal][paypal-shield]][paypal]
[![BuyMeCoffee][buymecoffee-shield]][buymecoffee]
[![Patreon][patreon-shield]][patreon]

## Introduction

This [ESPHome External Component](https://esphome.io/components/external_components) lets you
control an AXA Remote window opener by wire.

<img src="AXA Remote.jpg"/>

Features:
- Open/Stop/Close the window
- Move window to a given position
- Calibrate the open/close timings
- Use remote control simultaneously

### About the AXA Remote

The AXA Remote is a chain actuator driven electronic window opener that comes with an infrared
remote controll. It is mounted in the window frame.

## AXA Remote models

### AXA Remote RV2900

The original AXA Remote what was introduced in 2008 or earlier. It has a plastic frame and housing.
It is unknow if it can be controlled by wire. If you have more information on this device let me
know!

### AXA Remote 2.0

The successor to the original AXA Remote introduced in 2011(?). It has increased burglar resistance.
The frame is made of zamac, a zinc aluminium alloy while the housing remains plastic. It uses two
motors, one to unlock and an other to open the window. This is the model that I used to develop this
ESPHome External Component.

### AXA Remote 2.0S

A synchronised version AXA Remote 2.0 that keeps a second window opener in sync with a master
window operen. The intended use are large windows that need more power to open. The synchronisation
is done by sending synchronisation messages back and forth between the window openers. This ESPHome
External Component uses the same wired bus these synchronisation messages us and interferes with
these messages.

## Hardware required:

- ESP32 or other ESPHome supported micro controller
- LIN-bus controller or any other kind of level converter
- External power supply

The AXA Remote component depends on the ESPHome UART component. The ESP8266 has a software emulated
UART which has shown to be quite flaky. It is recomended to use a micro controller with a hardware
UART, like the ESP32, for increased accuracy. 

### Wiring

The AXA Remote has two identical RJ25 (6P6C) connectors, a pin on one connector is connected with
the same pin on the other connector. The pinout is as follows:

|Pin|Function|
|:-:|:------:|
| 1 |   Vin  |
| 2 |   GND  |
| 3 |  RX/TX |
| 4 |  RX/TX |
| 5 |   GND  |
| 6 |   Vin  |

Wiring a LIN-bus controller as level converter goes as follows:

<img src="schematic.png" width="50%"/>

### Powering the AXA Remote

The AXA Remote can be powered using 4 AA batteries or an external power supply which connects to
the RJ25 plug on the window opener. The outer pins of the RJ25 are the + voltage input and pin 2/5
are ground. The official AXA Remote power supply delivers 7.5 Volt, however the device also seems
to work fine on 9 Volt. The voltage does influence the speed and noise of the device, a lower
voltage runs the device slower and less noisy while a higher voltage faster and noisier.

While this ESPHome External Component should work with a battery powered AXA Remote when the ESP is
powered externally it makes more sense to power the AXA Remote using an external power supply and
power the ESP directly from the AXA Remote using a voltage regulator. Some voltage regulators on
ESP boards are actually quite tolerant regarding the input voltage and could be powered directly
from the AXA Remote input voltage. Your mileage may vary and do this at your own risk.

## PCB

I designed a PCB that fits in the battery bay of the AXA Remote and can controll 2 window openers.
Only minor modifications to the battery bay are needed. Contact me if you like to buy one! You can
find my email address on [my GitHub profile](https://github.com/rrooggiieerr).

<img src="PCB.png"/>

PCB in the battery bay:

<img src="PCB in battery bay.png"/>

## Protocol

This are the protocol details:  

Serial port settings: 19200 baud 8N2  

Device command: `DEVICE`, returns the device type, `AXA RV2900 2.0` on my device.  
Version command: `VERSION`, returns the firmware version of the AXA Remote.  
Status command: `STATUS`, returns the current lock state.  
Open command: `OPEN`, opens the window opener.  
Stop command: `STOP`, stops the window opener.  
Close command: `CLOSE`, closes the window opener.  
Help command: `HELP`, shows the available commands.

Each command is ended with a carriage return and newline `\r\n`.

### Lock states

- Unlocked
- Strong Locked
- Weak Locked

## Using the remote control

The AXA Remote only reports back if the device is locked or unlocked. When using the remote control
to open the window from a closed position the integration can detect this and report the state
accordingly. The same applies for closing the window with the remote control till a fully closed
position. However stopping the window or partially opening/closing the window with the remote
control can not be detected and the exact position the window is opened at will not correspond with
the real position.

## Quirks

The AXA Remote firmware has some nasty quirks. 

After a power outage the AXA Remote always reports the lock state as unlocked even when the window
is closed and locked. Only opening and then closing the window will reset the lock state.

After a power outage the AXA Remote will only close if first the open command is given even when
the window is fully open.

## ESPHome example configuration:
```
esphome:
  name: axaremote
  friendly_name: AXA Remote

external_components:
  - source: github://rrooggiieerr/esphome-axaremote
    refresh: 0s

esp32:
  board: esp32dev
  framework:
    type: esp-idf

# Enable logging
logger:
  level: INFO

# Enable Home Assistant API
api:
  encryption:
    key: ...

ota:
  - platform: esphome
    password: ...

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

uart:
  tx_pin: ... # To pin 4 of the MCP2003
  rx_pin: ... # To pin 1 of the MCP2003
  baud_rate: 19200
  stop_bits: 2

cover:
  - platform: axaremote
    name: "AXA Remote"
    close_duration: 50s
    auto_calibrate: True
```

### Configuration variables:

- __close_duration__ (Optional): The close duration from fully open to fully locked. From this time
  the unlock, open, close and lock durations are derived.
- __auto_calibrate__ (Optional): Enable/disable auto calibration of the unlock, open, close and
  lock durations.
- __polling_interval__ (Optional): The time between status update requests from the AXA Remote.
  Defaults to 0s (continuous polling). __auto_calibrate__ will not be accurate with a reduced
  polling interval.
- __retry_interval__ (Optional): The time between retries in case a command fails to be transmitted. Defaults to 0s.

### Model specific configuration for the AXA Remote 2.0S

To reduce the interference of this ESPHome External Component with the synchronisation messages of
the AXA Remote 2.0S it is recommended to use a reduced polling interval of 5 seconds or more and a retry interval of 1 second or more.

```
cover:
  - platform: axaremote
    name: "AXA Remote 2.0S"
    close_duration: 50s
    polling_interval: 5s
    retry_interval: 1s
```

### Calibration

This AXA Remote component for ESPHome can automatically calibrate the unlock, open, close and lock
durations. To do this open the window fully and close the window from the ESPHome device. The
component will measure the time it takes until the lock is in one of the locked states. By default
the close duration is logged as info message to the ESPHome logging console. You can then use this
value to set the __close_duration__ of your ESPHome configuration YAML. You can also enable
__auto_calibrate__ which will automatically update the unlock, open, close and lock durations every
time the window is closed from fully open to fully closed and locked.


## Star this external component

Help other ESPHome and AXA Remote users find this external component by starring this GitHub page.
Click **⭐ Star** on the top right of the GitHub page.

## Support my work

Do you enjoy using this ESPHome component? Then consider supporting my work using one of the
following platforms, your donation is greatly appreciated and keeps me motivated:

[![GitHub Sponsors][github-shield]][github]
[![PayPal][paypal-shield]][paypal]
[![BuyMeCoffee][buymecoffee-shield]][buymecoffee]
[![Patreon][patreon-shield]][patreon]

## Hire me

If you're in need for a freelance ESP developer for your project please contact me, you can find my
email address on [my GitHub profile](https://github.com/rrooggiieerr).

[releases]: https://github.com/rrooggiieerr/esphome-axaremote/releases
[releases-shield]: https://img.shields.io/github/v/release/rrooggiieerr/esphome-axaremote?style=for-the-badge
[license]: ./LICENSE
[license-shield]: https://img.shields.io/github/license/rrooggiieerr/esphome-axaremote?style=for-the-badge
[maintainer]: https://github.com/rrooggiieerr
[maintainer-shield]: https://img.shields.io/badge/MAINTAINER-%40rrooggiieerr-41BDF5?style=for-the-badge
[paypal]: https://paypal.me/seekingtheedge
[paypal-shield]: https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white
[buymecoffee]: https://www.buymeacoffee.com/rrooggiieerr
[buymecoffee-shield]: https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black
[github]: https://github.com/sponsors/rrooggiieerr
[github-shield]: https://img.shields.io/badge/sponsor-30363D?style=for-the-badge&logo=GitHub-Sponsors&logoColor=ea4aaa
[patreon]: https://www.patreon.com/seekingtheedge/creators
[patreon-shield]: https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white
