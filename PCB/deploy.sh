#!/bin/sh
#
# Shell script to deploy a bunch of PCBs with the latest firmware.
#

if [ "$1" != "" ]; then
	PORT=$1
else
	PORT=`ls -tr /dev/tty.* | tail -n 1`
fi

# Download latest firmware to temp file
TMPFILE=`mktemp`
curl -o "$TMPFILE" https://rrooggiieerr.github.io/esphome-axaremote/firmware/axa-remote-pcb-esp32.factory.bin

# Create and activate Python virtual environment
mkdir -p ~/.venv/esphome
python3.13 -m venv ~/.venv/esphome
source ~/.venv/esphome/bin/activate

while true; do
	read -n 1 -s -r -p "Press Q to quit or connect PCB to $PORT and any other key to flash a new PCB" RESPONSE
	if [ "$RESPONSE" == "q" ] || [ "$RESPONSE" == "Q" ]; then
		break
	fi

	# Erase ESP32
	esptool --port "$PORT" --chip esp32 erase_flash

	# Flash firmware
	esptool --port "$PORT" --baud 921600 write-flash 0x00 "$TMPFILE"
done

# Deactivate Python virtual environment
deactivate

# Delete temp file
rm "$TMPFILE"
