# Распиновка Guition ESP32-4848S040

Источники, по которым сверена таблица (все совпадают по GPIO):

- [FigueiredoStable/LovyanGFX-GUITION-ESP32-4848S040](https://github.com/FigueiredoStable/LovyanGFX-GUITION-ESP32-4848S040) — `src/LGFX_ESP32S3_RGB_TFT_SPI_ST7701_GT911.h`
- [sand1812/ESP32-4848S040](https://github.com/sand1812/ESP32-4848S040) — `src/main.cpp`
- [aquaElectronics/esp32-4848s040-st7701](https://github.com/aquaElectronics/esp32-4848s040-st7701) — `src/display.cpp` (MIT)
- [PrayerfulDrop/ESPHome-Guition-ESP32-S3-4848S040](https://github.com/PrayerfulDrop/ESPHome-Guition-ESP32-S3-4848S040) — `Guition-ESP32/hardware.yaml`

## ST7701S 16-bit RGB

| Сигнал | GPIO | Примечание |
|---|---:|---|
| DE | 18 | data enable |
| VSYNC | 17 | polarity 1 |
| HSYNC | 16 | polarity 1 |
| PCLK | 21 | 12 МГц (проверенный режим) |
| R0 | 11 | |
| R1 | 12 | |
| R2 | 13 | |
| R3 | 14 | |
| R4 | 0 | strapping |
| G0 | 8 | |
| G1 | 20 | native USB D+, занят |
| G2 | 3 | strapping |
| G3 | 46 | strapping |
| G4 | 9 | |
| G5 | 10 | |
| B0 | 4 | |
| B1 | 5 | |
| B2 | 6 | |
| B3 | 7 | |
| B4 | 15 | |

Тайминги (Arduino_GFX):

```text
hsync_polarity=1, front=10, pulse=8,  back=50
vsync_polarity=1, front=10, pulse=8,  back=20
pclk_active_neg=0, prefer_speed=12 MHz
de_idle_high=0, pclk_idle_high=0
bounce_buffer_size_px = 480*16   // PSRAM
```

Более высокий PCLK (14 МГц) встречается в LovyanGFX-конфиге, но 12 МГц
стабильнее для Arduino_GFX + bounce. Менять только при подтверждённых
артефактах и с проверкой питания/шлейфа.

## ST7701 init (3-wire SPI)

| Сигнал | GPIO |
|---|---:|
| CS | 39 |
| SCK | 48 |
| MOSI/SDA | 47 |

Полная init-последовательность — `src/hal_esp32/display_st7701.cpp`
(из MIT-примера aquaElectronics).

## Подсветка

| Сигнал | GPIO |
|---|---:|
| BL | 38 |

LEDC, 1 кГц, 10 бит. 0 — выключено. На время OTA/upload подсветка гасится.

## GT911 (I2C)

| Сигнал | GPIO |
|---|---:|
| SDA | 19 |
| SCL | 45 |
| INT | не разведён |
| RST | не разведён |

Адреса 0x5D / 0x14, частота 100 кГц. Драйвер polling, без reset-пина.
Координаты 0..479 без зеркалирования.

## microSD (TF)

| Сигнал | GPIO |
|---|---:|
| CS | 42 |
| MISO | 41 |
| MOSI | 47 |
| SCK | 48 |

Карта работает в SPI-режиме, FAT32. `MOSI/SCK` совпадают с шиной инициализации
ST7701, но дисплей использует программный SPI только один раз в
`display_begin()`; после этого пины отдаются SD. Card-detect на GPIO не
разведён, поэтому вставка/извлечение определяются опросом монтирования.

Источники: [HomeDing ESP32-4848S040](https://homeding.github.io/boards/esp32s3/panel-4848S040.htm)
и [northernman54/ESP32-4848S040](https://github.com/northernman54/esp32-4848s040).
На других ревизиях платы пины следует перепроверить.

## USB / UART

GPIO 19 и 20 заняты тачем и G1, поэтому native USB ESP32-S3 недоступен.
`ARDUINO_USB_CDC_ON_BOOT=0`; Serial и прошивка — через UART-мост платы.

## Strapping

GPIO 0, 3, 45, 46 — strapping-пины. Не менять их режим при старте без
необходимости; 45 (SCL) и 46 (G3) уже используются платой.
