# Greenhouse Monitor

An ESP32-based IoT system that monitors greenhouse environmental conditions (temperature, humidity, CH4, CO2) and transmits sensor data over WiFi to a remote server for logging and analysis.

## Architecture

```
  Greenhouse                        Server
 ┌─────────────────────┐
 │  DHT11 ──(GPIO 25)──┤
 │  MQ-4  ──(ADC CH6)──├── ESP32 ──── WiFi ────── TCP Server ──── data_log.csv
 │  MQ-135 ─(ADC CH7)──┤   (binary, 24 bytes)        │
 └─────────────────────┘                          Processing
        powered via                               Script
      NPN 2N2222 switch                        (outlier detection,
        (GPIO 27)                               median analysis)
```

## Key Metrics

| Metric | Value |
|---|---|
| Active power draw (WiFi on) | ~1250 mW (250 mA @ 5V) |
| Deep sleep power draw | ~0.05 mW (10 uA @ 5V) |
| Battery life (continuous) | ~4.5 hours |
| Battery life (deep sleep cycle) | **~5.3 days** |
| Deep sleep interval | 30 minutes |
| Active window per cycle | ~3.5 min (3 min warmup + 30s measure/transmit) |
| Data packet size | 24 bytes (binary struct) |
| Sensor calibration | Logarithmic interpolation from datasheet curves |
| Total BOM cost | ~135 RON |

## Hardware

| Component | Qty | Purpose |
|---|---|---|
| ESP-WROOM-32 | 1 | Microcontroller with WiFi |
| DHT11 | 1 | Temperature and humidity |
| MQ-4 | 1 | Methane (CH4) detection |
| MQ-135 | 1 | CO2 detection |
| 2N2222 NPN transistor | 1 | Sensor power switch for deep sleep |
| 1.2V 2500mAh batteries | 4 | Power supply (series, 4.8V) |
| Resistors | 6 | Pull-up (5k for DHT11) and voltage dividers |

The MQ sensors operate at 5V and output through voltage dividers since the ESP32 ADC accepts max 3.3V. The DHT11 runs directly at 3.3V with a 5k pull-up resistor. An NPN transistor on GPIO 27 cuts power to all sensors during deep sleep.

## Power Analysis

In continuous mode, the system draws ~551 mA total:
- ESP32 with WiFi: 250 mA
- MQ-4 heater (33 ohm @ 5V): 150 mA
- MQ-135 heater (33 ohm @ 5V): 150 mA
- DHT11: ~1 mA

With deep sleep, the device wakes once per hour for ~3.5 minutes (sensor warmup + measurement + transmission), consuming only ~19.63 mAh/hour. This extends battery life from **~4.5 hours to ~5.3 days** on 4x 1.2V 2500 mAh batteries.

## Tech Stack

- **Firmware**: C, ESP-IDF (FreeRTOS), bare-metal sensor drivers (DHT11 bit-banging, MQ analog reading)
- **Communication**: TCP sockets, 24-byte binary protocol (`struct: iiffq`), NTP time sync
- **Server**: Python, TCP socket server, CSV logging
- **Analysis**: pandas (median calculation, IQR outlier detection)

## Building

### Firmware

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html).

```bash
idf.py build
idf.py flash
```

### Optimizations

The following `idf.py menuconfig` flags reduce startup and deep-sleep wake time:

- `CONFIG_ESPTOOLPY_FLASHFREQ` = 80 MHz
- `CONFIG_ESPTOOLPY_FLASHMODE` = QIO
- `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP`
- `CONFIG_COMPILER_OPTIMIZATION` = -O2
- Logs disabled in production mode

### Server

```bash
cd scripts/
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

python server.py      # start TCP server (listens on 0.0.0.0:1234)
python process.py     # analyze collected data
```

## Future Improvements

- Bluetooth provisioning for WiFi credentials
- SD card for offline data logging when server is unreachable
- Higher precision sensors (e.g., DHT22, BME280)
- 3.3V native sensors or level shifters to eliminate voltage dividers
- RTC module to avoid NTP dependency on each wake cycle
