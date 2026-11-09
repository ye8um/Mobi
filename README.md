# Mobi — Phone-Controlled Two-Wheel Robot

Mobi is a compact, custom-designed Wi-Fi robot with an animated 0.96-inch OLED face. I designed its PCB in KiCad and its rounded enclosure, motor mounts, display opening, and power-bank compartment in Fusion 360. The robot uses a Seeed XIAO ESP32-C3, a TB6612FNG dual motor driver, and two small N20 gear motors. A small ball caster supports the body behind its drive wheels.

The goal is a simple, friendly robot that I can steer from my phone without installing an app. Mobi creates a Wi-Fi network and serves a web controller with directional buttons and a speed slider. The motor control stops when the button is released or commands stop arriving. Its OLED face blinks and shifts its eyes while turning.

## Features

- Phone control over a local Wi-Fi access point, no internet needed
- Forward, reverse, left, right, and stop
- Adjustable speed using PWM motor control
- Animated 128 × 64 OLED eyes
- 450 ms command timeout for a basic motor fail-safe
- Custom two-layer PCB and 3D-printed enclosure
- Two small N20 geared motors and a passive rear ball caster

There is **no obstacle or edge sensor** in this version, so operate it only on a level, clear floor, away from stairs, edges, and fragile objects. This is a hobby robot, not an autonomous robot.

## Hardware overview

Main controller: Seeed XIAO ESP32-C3 with its external Wi-Fi antenna. Motor controller: **bare** TB6612FNG SSOP-24 soldered to the custom PCB; not the breakout module. The TB6612's PWMA, PWMB, and STBY inputs are tied to 3.3 V in this board design. Display: common SSD1306 0.96-inch 128 × 64 **4-wire I2C** module, address usually 0x3C. Power: a commercial 5 V USB power bank, a **proper USB-C 5 V sink breakout**, an adequately rated power switch, and a Schottky diode feeding the XIAO VBUS input.

### Firmware pin map — updated after the pin swap

| XIAO label | GPIO | Function |
|---|---:|---|
| D1 | 3 | TB6612 AIN1 |
| D2 | 4 | TB6612 AIN2 |
| D3 | 5 | TB6612 BIN1 |
| D4 | 6 | TB6612 BIN2 (swapped) |
| D6 | 21 | OLED SDA (swapped) |
| D5 | 7 | OLED SCL |
| 3V3 | — | TB6612 VCC/PWMA/PWMB/STBY and compatible OLED VCC |
| GND | — | Common logic and motor ground |

Connect TB6612 AO1/AO2 to left motor and BO1/BO2 to right motor. Tie all TB6612 motor-supply VM and PGND pads correctly. The motor supply comes from the switched **5 V power input**, not from the XIAO 3.3 V output. The driver requires local decoupling as documented in its datasheet.

The XIAO's 5 V/VBUS supply must follow Seeed's external supply diode guidance. Do not connect the powered robot to your computer's USB port at the same time as its external 5 V source without reviewing the power arrangement.

## Arduino setup

1. Install Arduino IDE and Espressif's `esp32` board package.
2. Choose the **Seeed XIAO ESP32C3** board and its USB port.
3. From Arduino Library Manager install **Adafruit GFX Library** and **Adafruit SSD1306** (with their dependencies).
4. Open `Arduino/Mobi.ino`.
5. Check the pin assignments against the **latest actual KiCad schematic**, especially the changed D4/D6 pins.
6. Change `AP_PASSWORD` in the sketch. Upload the sketch with the robot's external power disconnected.
7. Test with the robot raised so the drive wheels cannot touch the floor. Start at low speed and verify both motor directions and the OLED.
8. Power the assembled robot, connect your phone to `Mobi-Robot`, then open `http://192.168.4.1`.
9. Press and hold a direction button to drive. Release to stop. The STOP button works immediately; a 450 ms timeout covers dropped commands.

If one motor turns backward, set `REVERSE_LEFT` or `REVERSE_RIGHT` to `true` in the sketch, then upload again. If the OLED remains blank, confirm its 3.3 V compatibility, 0x3C I2C address, SDA/SCL wires, and that it really uses an SSD1306 controller.

## CAD and assembly

The current concept has a 120 × 95 mm rounded chassis, a roughly 65 × 50 mm PCB, 2 mm outer walls, and a removable top cover. The display window is in the front. The battery-holder placeholder is 75 × 40 × 15 mm, **not a confirmed power-bank model**. Do not treat that placeholder as a purchase specification.

The motor CAD references are GA12-N20-sized, but must be compared against the purchased motor shaft and bracket dimensions. Wheel diameter changes the axle height, and the caster must touch the same floor plane as both wheels. For a 32 mm wheel and 8 mm chassis clearance, the axle center must be 8 mm above the chassis bottom. The earlier 24.5 mm shaft height assumed a 65 mm wheel, so it is **not compatible** with 32 mm wheels without adjusting the design.

Use the actual PCB STEP file for hole spacing, USB cutout location, and enclosure fit. Verify mounting screws, switch actuator clearance, PCB antenna keepout, and wire bends before printing the final body.

## Required files

- `Arduino/Mobi.ino`: complete phone-control firmware source (not hardware-tested)
- `BOM.csv` / `Mobi_BOM.xlsx`: all hardware, sourcing and validation notes
- `docs/Build_Checklist.md`: last checks before PCB order and assembly

KiCad and Fusion native source files are **not** in this package because the final versions were not provided for export; use your own saved project files.

## Current build status

The firmware is written, but it has **not been compiled on your machine or tested on actual Mobi hardware**. The selected main-power switch and USB-C input module require exact-part confirmation, and the power bank must be measured before the enclosure is finalized. The BOM separates confirmed product listings from estimates and placeholders. A custom PCB should not be ordered solely based on the firmware or generic links.

## Credits and references

- Seeed XIAO ESP32-C3 hardware guide: https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
- Toshiba TB6612FNG IC listing and datasheet: https://www.digikey.com/en/products/detail/toshiba-semiconductor-and-storage/TB6612FNG-C-8-EL/1730070
- Pololu 3/8-inch ball caster: https://www.pololu.com/product/951
- Pololu 32 mm wheel pair: https://www.pololu.com/product/1087
- ElectroPeak GA12-N20 motor/wheel kit listing: https://electropeak.com/n20-gear-motor-with-wheel

Mobi is my personal electronics and mechanical design project, with code and guidance developed for this build.
