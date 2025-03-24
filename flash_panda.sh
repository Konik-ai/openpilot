#!/bin/bash
set -e

# prevent LIBUSB_ERROR_ACCESS [-3]
sudo chmod -R 777 /dev/bus/usb/

# recover
poetry run panda/board/recover.py

# flash
poetry run panda/board/flash.py
