
# Wi-Fi configuration

The ESPHome based firmware implements the open _Improv via BLE_ standard for configuring Wi-Fi. If
your Home Assistant has Bluetooth configured, or you're using _[Bluetooth Proxies](https://esphome.io/components/bluetooth_proxy/)_,
the AXA Remote PCB will be automatically discovered by the Improv integration. 

<img src="PCB discovered by Improv via BLE.png" width="50%"/>

You can then continue the _Improv via BLE_ config flow to setup the Wi-Fi connection for the ESP32.

<img src="Improv via BLE confirm set up.png" width="50%"/> <img src="Improv via BLE Wi-Fi credentials.png" width="50%"/>

When using Improv Wi-Fi via BLE you need to press the __Boot__ button on the PCB to authorize the
authentication.

ToDo photo of Boot button.

<img src="Improv via BLE authorisation.png" width="50%"/>

If you don't have Bluetooth configured you can connect to the built in fallback hotspot named
_axa-remote-000000_ and configure the Wi-Fi using the supplied web interface on
[http://192.168.4.1/](http://192.168.4.1/).
