# Hardware Installation

To fit the PCB in the battery compartment of the AXA Remote two minor modifications are needed.

<img src="PCB in battery compartment.png"/>

First the battery spring needs to be removed, you can easily slide this out of the battery
compartment.

ToDo Photo of battery spring.

Then a small notch needs to be cut out the battery compartment to guide the wire towards the cable
duct that brings the cable to the RJ25 (6P6C) connector of the AXA Remote.

ToDo Photo of notch.

Connect the RJ25 (6P6C) side of the provided cable to the _AXA Remote_ and guide the wire through the
cable duct. Then connect the JST XH connector to __J2/AXA 1__ on the PCB.

__J3/AXA 2__ can be used to connect a second AXA Remote. By default the power of the primary AXA Remote
is forwarded to the secondary, but if this secondary AXA Remote has it's own power supply you can
cut __JP2__ and __JP3__ to disable the power forwarding.

ToDo Photo of __JP2__ and __JP3__.

The firmware will detect if a second AXA Remote is connected and create a cover entity accordingly.

## Connecting the Light-Dependent Resistor (LDR)

Optionally you can solder the provided LDR on solderpads __R9/Brightness__ of the PCB. The LDR does
not have a polarity, orientation does not matter. You can drill a 5 mm hole in the casing of the
AXA Remote and let the LDR peek through.

ToDo Photo of installed LDR.

The light sensor is disabled in Home Assistant. Click the disabled entity and enable the sensor if
you want to use the sensor.

## Disabling the power LED

You can disable the Power LED by cutting the trace between jumper __JP1__ with a sharp knife. If
you want to enable the LED again on a later moment soldering __JP1__ will bring the LED back.

