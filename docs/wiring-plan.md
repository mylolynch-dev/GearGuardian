# Gear Guardian — Wiring Plan

## ESP32-S3 DevKitC-1 Pin Assignments

> **Status**: All assignments marked "TODO: confirm" must be verified against
> your physical wiring before flashing.  Pin numbers are centralized in
> `boards/esp32s3_devkitc.overlay` — update them there and nowhere else.

---

## Pin Table

| Signal | ESP32-S3 GPIO | DT Node / Alias | Direction | Notes |
|--------|--------------|-----------------|-----------|-------|
| ICM-20948 SDA | GPIO8 | `&i2c0` SDA | Bidirectional | I2C0 default. TODO: confirm pinctrl |
| ICM-20948 SCL | GPIO9 | `&i2c0` SCL | Output | I2C0 default. TODO: confirm pinctrl |
| OLED MOSI | GPIO11 | `&spi2` MOSI | Output | SPI2 (HSPI) |
| OLED SCLK | GPIO12 | `&spi2` CLK | Output | SPI2 (HSPI) |
| OLED CS | GPIO10 | `ssd1306@0 cs-gpios` | Output, active-low | Software CS |
| OLED DC | GPIO13 | `ssd1306@0 dc-gpios` | Output, active-high | Data/command select |
| OLED RESET | GPIO14 | `ssd1306@0 reset-gpios` | Output, active-low | Hold low for reset |
| SD MOSI | GPIO35 | `&spi3` MOSI | Output | SPI3 (VSPI). TODO: confirm |
| SD MISO | GPIO37 | `&spi3` MISO | Input | SPI3 (VSPI). TODO: confirm |
| SD SCLK | GPIO36 | `&spi3` CLK | Output | SPI3 (VSPI). TODO: confirm |
| SD CS | GPIO38 | `sdhc@0 cs-gpios` (`gpio1` pin 6) | Output, active-low | 38-32=6 |
| Reed switch | GPIO4 | `reed-switch` alias | Input, active-low, pull-up | Normally-closed switch |
| Mode button | GPIO0 | `mode-btn` alias | Input, active-low, pull-up | DevKitC BOOT button |
| Buzzer | GPIO5 | `buzzer` alias | Output, active-high | Active buzzer module |
| Status LED | GPIO48 | `led0` alias (`gpio1` pin 16) | Output, active-high | Onboard RGB LED (blue) |

---

## Schematic Notes

### ICM-20948 IMU (SparkFun breakout)
- Power: 3.3V (breakout has onboard regulator; check jumper for 3.3V mode)
- AD0 pin: connect to VCC (3.3V) → I2C address = 0x69
- INT pin: not connected in V1 (polling mode)
- SDA/SCL: connect with 4.7kΩ pull-ups to 3.3V if not on breakout

### SSD1306 OLED (SPI variant)
- Power: 3.3V
- Interface: SPI (4-wire: MOSI, SCLK, CS, DC) + RESET GPIO
- Verify your module's pin order — some modules label D0=CLK, D1=MOSI

### microSD module (SPI)
- Power: 3.3V (most SPI SD modules have onboard voltage dividers for MISO)
- Format card as FAT32 before first use
- MISO: some modules require 10kΩ pull-up

### Reed switch
- Wired normally-closed (NC) with a pull-up resistor (or use internal pull-up)
- One side to GPIO4, other side to GND
- Magnet present = switch closed = GPIO4 LOW
- Magnet absent = switch open = GPIO4 HIGH (pulled up)

### Active buzzer
- Verify your buzzer module's drive current requirements
- ESP32-S3 GPIO max source current: ~40 mA
- If buzzer draws more than 10 mA, add an NPN transistor driver (e.g. 2N2222)
  with a base resistor (1kΩ) from GPIO5 and a flyback diode

---

## ESP32-S3 GPIO Bank Notes

The ESP32-S3 has two GPIO banks in Zephyr's device tree:
- `&gpio0`: GPIO pins 0–31
- `&gpio1`: GPIO pins 32–47 (index = pin - 32)

Example: GPIO38 → `<&gpio1 6 ...>` (38 - 32 = 6)
Example: GPIO48 → `<&gpio1 16 ...>` (48 - 32 = 16)

---

## TODO: Confirm Before First Flash

1. Verify I2C0 SDA/SCL pinctrl in `esp32s3_devkitc-pinctrl.dtsi` matches GPIO8/GPIO9
2. Verify SPI2 and SPI3 pin assignments in the ESP32-S3 DevKitC board file
3. Test ICM-20948 I2C scan at 0x69 with `i2cdetect` or a probe sketch
4. Measure buzzer drive current; add transistor if needed
5. Verify SD card is FAT32 formatted and detectable
6. Confirm OLED module pinout matches overlay CS/DC/RESET assignments
