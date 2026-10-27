# SAP · SwitchAsPad

English | [简体中文](README.zh-CN.md)

Use a Nintendo Switch or Switch Lite as a controller for a Linux PC over USB or Wi-Fi, including emulators and other games with gamepad support.

![Interface preview](docs/preview.png)

## Features

- ABXY, D-pad, both analog sticks and stick clicks, L/R/ZL/ZR, and Plus/Minus.
- USB and Wi-Fi input, with USB preferred when both are available.
- Releases virtual controller inputs after 300 ms without data.
- Displays time, battery level, external power, connection status, and live input on a fixed 1280×720 canvas with uniform scaling.
- Hold Plus and Minus together for two seconds to exit. Shoulder buttons remain functional even though they are not drawn.

The PC receiver currently supports **Linux only**. Windows, macOS, motion controls, and rumble are not supported. The Switch must be able to run homebrew NRO applications. This is not driverless USB HID firmware.

The English screenshot is rendered from the UI code in preview mode. The current Switch application uses Chinese interface text.

## Switch setup

Build `switchaspad.nro` from source and copy it to the SD card:

```text
/switch/switchaspad/switchaspad.nro
```

For Wi-Fi, create `pc.txt` in the same directory containing only your PC's LAN IPv4 address, such as `192.168.1.100`. Without this file, the application uses LAN broadcast. Connect both devices to the same network and allow UDP port 26780. For USB, use a data-capable cable.

Exit DBI or other applications using USB, then launch SwitchAsPad from hbmenu.

## Linux receiver

On Debian or Ubuntu:

```bash
sudo apt install python3-venv libusb-1.0-0 acl
python3 -m venv .venv
.venv/bin/pip install -r pc/requirements.txt
sudo modprobe uinput
sudo setfacl -m "u:$(id -un):rw" /dev/uinput
```

USB mode also needs device access. Install a udev rule:

```bash
printf '%s\n' 'SUBSYSTEM=="usb", ATTR{idVendor}=="057e", ATTR{idProduct}=="3001", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-switchaspad.rules
sudo udevadm control --reload-rules
```

Reconnect the USB cable, then start the receiver:

```bash
.venv/bin/python pc/receiver.py
# Or select a single transport:
.venv/bin/python pc/receiver.py --transport wifi
.venv/bin/python pc/receiver.py --transport usb
```

You may need to repeat the `setfacl` command after rebooting or recreating the device. UDP has no pairing or authentication; use it on a trusted LAN and do not forward the port to the internet.

## Emulator configuration

Select **SwitchAsPad** as the input device and **Pro Controller** as the emulated controller type. Check ABXY mappings against the physical buttons. Map Plus to **Start** and Minus to **Back**. Refresh the device list or restart the emulator if the controller is missing.

## Build the NRO

Install Docker. The official devkitPro image includes the libnx and SDL2 libraries used here:

```bash
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64:latest make -j4
```

The output is `switchaspad.nro` in the repository root. The `latest` image tag is mutable, so future toolchain updates may affect builds.

## Render a desktop preview

```bash
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-gfx-dev
cc -DSAP_PREVIEW_EN tools/preview.c source/ui.c -o preview $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_gfx) -lm
SDL_VIDEODRIVER=dummy ./preview
```

This produces `preview.bmp` with example time, battery, and input values. Pass `0` for disconnected, `1` for Wi-Fi, or `2` for USB. Omit `-DSAP_PREVIEW_EN` to render the Chinese interface used on the Switch.

## Source layout

- `source/main.c`: libnx input, system status, USB and UDP transport.
- `source/ui.c`: dynamic UI, fonts, and proportional display scaling.
- `romfs/layout.svg`: static vector artwork; `layout.bmp` is the raster version embedded in the NRO.
- `pc/receiver.py`: Linux uinput gamepad and dual-transport receiver.

Reports use the 24-byte little-endian structure `<4sIQhhhh`: `SPAD`, a 32-bit sequence number, 64-bit libnx button flags, and four signed 16-bit stick values. UDP uses port 26780 and the acknowledgement `SPAK`. USB uses VID/PID `057e:3001`, inherited from the prototype; these are not USB IDs allocated to this project.

## Validation status

Physical USB input, the Linux virtual gamepad, and timeout-based input release have been tested on the development machine. The current UI has passed desktop rendering checks and NRO compilation. Its latest appearance on Switch Lite still needs hardware confirmation. Compatibility with every firmware, host, or emulator configuration is not established.

## License

Project code and original UI assets are licensed under [MIT](LICENSE). `romfs/ui.otf` and `romfs/bold.otf` are subsets derived from Adobe Source Han Sans SC and are licensed separately under the [SIL Open Font License 1.1](romfs/OFL.txt), not MIT. libnx, SDL2, SDL2_ttf, SDL2_gfx, and their dependencies retain their respective upstream licenses.
