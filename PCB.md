# PCB for AXA Remote

[![GitHub Release][releases-shield]][releases]
[![Licence][license-shield]][license]
[![Maintainer][maintainer-shield]][maintainer]  
[![GitHub Sponsors][github-shield]][github]
[![PayPal][paypal-shield]][paypal]
[![BuyMeCoffee][buymecoffee-shield]][buymecoffee]
[![Patreon][patreon-shield]][patreon]

## Introduction

I designed a PCB that fits in the battery compartment of the AXA Remote and can control up to two
window openers. Only minor modifications to the battery compartment are needed. Contact me if you
like to buy one! You can find my email address on [my GitHub profile](https://github.com/rrooggiieerr).

<img src="PCB.png"/>

### Features

- Fits in the AXA Remote battery compartment  
  Two slits in the PCB allow you to slide the PCB over the battery deviders in the battery
  compartment.
- Control up to two AXA Remote window openers  
  Two LIN convertors allow you to connect to two different AXA Remote window openers.
- Powered from the AXA Remote power supply  
  A buit in power regulator brings the 7.5 Volt power of the AXA Remote to the 3.3 Volt that the
  ESP32 needs.
- Power pass trough to second window opener  
  The connector for connecting to a second window opener als delivers power, no need for a second
  power supply.
- Light sensor  
  A provided photoresistor can be soldered in place to make the firmware report light intensity.
- Easy connection to your local Wi-Fi network.  
  Implements a captive portal and the open [Improv Wi-Fi](https://www.improv-wifi.com/) standard
  via BLE to easily setup your local Wi-Fi network credentials.

## What's included

- PCB
- 40 cm RJ25 (6P6C) to JST XH connector cable  
  To connect the AXA Remote to the PCB.
- RJ25 (6P6C) connector and 3 pin female JST XH connector + contacts  
  To make your own wire for connection a second AXA Remote.
- Light-Dependent Resistor (LDR)  
  To measure brightness.

## Installation

### Hardware

To fit the PCB in the battery compartment of the AXA Remote two minor modifications are needed.

<img src="PCB in battery compartment.png"/>

First the battery spring needs to be removed, you can easily slide this out of the battery
compartment.

Then a small notch needs to be cut out the battery compartment to guide the wire towards the cable
duct that brings the cable to the RJ25 (6P6C) connector of the AXA Remote.

Connect the RJ25 (6P6C) side of the provided cable to the _AXA Remote_ and guide the wire trough the
cable duct. Then connect the JST XH connector to _J2/AXA 1_ on the PCB.

_J3/AXA 2_ can be used to connect a second AXA Remote. By default the power of the primary AXA Remote
is forwarded to the secondary, but if this secondary AXA Remote has it's own power supply you can
cut JP3 and JP3 to disable the power forwarding.

### Connecting the Light-Dependent Resistor (LDR)

Optionally you can solder the provided LDR on solderpads _R9/Brightness_ of the PCB. The LDR does
not have a polarity, orientation does mot matter. You can drill a 5 mm hole in the casing of the
AXA Remote and let the LDR peek trough.

The light sensor is disabled in Home Assistant. Click the disabled entity and enable the sensor if
you want to use the sensor.

### Wi-Fi configuration

The ESPHome based firmware implements the open _Improv via BLE_ standard for configuring Wi-Fi. If
your Home Assistant has Bluetooth configured, or you're using [Bluetooth Proxies](https://esphome.io/components/bluetooth_proxy/),
the AXA Remote PCB will be automatically discovered by the Improv integration. 

<img src="PCB discovered by Improv via BLE.png" width="50%"/>

You can then continue the _Improv via BLE_ config flow to setup the Wi-Fi connection for the ESP32.

<img src="Improv via BLE confirm set up.png" width="50%"/> <img src="Improv via BLE Wi-Fi credentials.png" width="50%"/>

When using Improv Wi-Fi via BLE you need to press the boot button to authorize the authentication.

<img src="Improv via BLE authorisation.png" width="50%"/>

If you don't have Bluetooth configured you can connect to the built in access pont named
_axa-remote-000000_ with password _axaremote_ and configure the Wi-Fi using the supplied web
interface on [http://192.168.4.1/](http://192.168.4.1/).

### Home Assistant configuration

After completing the Wi-Fi configuration the ESPHome integration will automatically discover the
AXA Remote PCB if the PCB is on the same network as your Home Assistant. You can then follow the
ESPHome configuration flow to add the PCB to your Home Assistant.

<img src="PCB discovered by ESPHome.png" width="50%"/>

## Updating the firmware

The PCB comes with a version of ESPHome and the AXA Remote component installed and configured for
one AXA Remote. If you want to enable the support for a second AXA Remote you need to update the
firmware.

ToDo

### ESPHome example configuration

```yaml
substitutions:
  name: "axa-remote-4131b0"
  friendly_name: AXA Remote 4131b0
packages:
  rrooggiieerr.axaremote: github://rrooggiieerr/esphome-axaremote/axa-remote-pcb.yaml
esphome:
  name: ${name}
  name_add_mac_suffix: false
  friendly_name: ${friendly_name}
api:
  encryption:
    key: ...

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

### Setting the close duration

By default the firmware is configured to auto calibrate the unlock, open, close and lock durations
every time the window is closed from fully open to fully closed and locked. If you prefer to set a
fixed time for your window you can override this configuration. To do this open the window fully
and the close the window. The component will measure the time it takes until the lock is in one of
the locked states. By default the close duration is logged as info message to the ESPHome logging
console. You can then use this value to set the __close_duration__ of your ESPHome configuration.

```yaml
cover:
  - id: !extend cover_axa1
    auto_calibrate: False
    close_duration: 35s
  - id: !extend cover_axa2
    auto_calibrate: False
    close_duration: 35s
```

### Adding BLE Tracker and Bluetooth Proxy

The ESP32 used on the PCB has Bluetooth capabilities and can thus be used as a BLE Tracker or
Bluetooth Proxy.

To enable the BLE Tracker component add the following line to the ESPHome configuration file.

```yaml
esp32_ble_tracker:
```

To enable the Bluetooth Proxy component add the following line to the ESPHome configuration file.

```yaml
bluetooth_proxy:
```

### GPIO

ToDo

## Factory reseting the firmware

A factory reset erases the settings stored in the ESP32.

To reset press the Boot button in the following sequence:

On for 2 second  
Off for 1 second  
On for 2 second  
Off for 1 second  
On for 5 seconds  

If successfull the Status LED will flash five times.

## Contribution and appreciation

You can contribute to this component, or show your appreciation, in the following ways.

### Star this external component

Help other ESPHome and AXA Remote users find this external component by starring this GitHub page.
Click **⭐ Star** on the top right of the GitHub page.

### Support my work

Do you enjoy using this ESPHome component? Please consider supporting my work through one of the
following platforms, your donation is greatly appreciated and keeps me motivated:

[![GitHub Sponsors][github-shield]][github]
[![PayPal][paypal-shield]][paypal]
[![BuyMeCoffee][buymecoffee-shield]][buymecoffee]
[![Patreon][patreon-shield]][patreon]

### ESPHome support

[Book a one-hour ESPHome support session](https://buymeacoffee.com/rrooggiieerr/e/470127). I’ll
help you troubleshoot your ESPHome setup or answer your ESPHome-related questions.

What can be done in one hour:

- ESPHome walktrough, I explain to you how ESPHome works
- Assistance setting up your ESP device
- Install and configure an ESPHome (External) Component

What takes more time:

- Support for ESPHome Component developers

### Hire me

If you would like to have an ESPHome component developed for your product or are in need for a
freelance ESP developer for your project please contact me, you can find my email address on
[my GitHub profile](https://github.com/rrooggiieerr).

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
