# Архитектура tablo

## Слои

```text
┌──────────────────────────────────────────────┐
│ src/ui — LVGL 9.3, только C++/LVGL           │
│   ui.cpp, dashboard, settings, schedule,     │
│   modal, theme, fonts                        │
└───────────────┬──────────────────────────────┘
                │ AppState (snapshot), IPlatform
┌───────────────▼──────────────────────────────┐
│ src/core — без Arduino/ESP-IDF               │
│   AppState, Settings, StateStore, IDataProv. │
│   JsonDoc/jsmn, parsers, version compare     │
└───────────────┬──────────────────────────────┘
        ┌───────┴────────┐
        │                │
┌───────▼─────────┐  ┌───▼────────────────────┐
│ hal_esp32       │  │ hal_wasm                │
│ display/touch   │  │ SDL2 canvas             │
│ WiFi/web/OTA    │  │ mock_data.json          │
│ NVS/rescue      │  │ platform_wasm           │
└─────────────────┘  └─────────────────────────┘
```

UI не знает, какая платформа под ним. Действия (яркость, night mode,
проверка/установка OTA, reboot) идут через `IPlatform`; данные — через
`StateStore` и `AppState`.

## Потоки ESP32

| Задача | Ядро | Приоритет | Что делает |
|---|---:|---:|---|
| render (`lvgl`) | 1 | 2 | `lv_timer_handler`, тач, `ui_update` раз в 200 мс |
| net | 0 | 1 | web-сервер, Wi-Fi, NTP, HTTP(S), OTA, ping, режим экрана |
| loop (Arduino) | 1 | 1 | boot guard, debounced NVS, microSD hot-plug |

Правила:

- LVGL трогает только render-задача.
- Сеть и web — только net-задача; TLS-сессии последовательные.
- На время записи flash: `render_pause()` + `backlight_set(0)`, после ошибки
  `render_resume()` + восстановление яркости. Успешный OTA перезагружает плату.
- `StateStore` защищён `std::mutex`; UI читает snapshot, сеть пишет целый
  `AppState`.

## Эмуляция и тесты

- `src/hal_wasm/` — интерактивное WASM-превью 480×480 с тачем для браузера;
  ручная проверка через `make preview`, не автоматический тест.
- `tests/ui/` — headless native LVGL smoke test (`make test-ui`): те же
  `src/ui`/`src/core`, `preview/mock_data.json`, framebuffer в памяти,
  последовательный обход dashboard → settings → OTA modal.
- `scripts/test_qemu.sh` (`make test-fw`) — реальный `firmware.bin` в
  Espressif QEMU. QEMU не моделирует Wi-Fi, lwIP, GT911 и microSD, поэтому прошивка
  определяет его по сигнатуре в APB_CTRL, не входит в блокирующий captive
  portal, пропускает I2C-тач и не поднимает web-сервер. На реальной плате
  эти ветки не выполняются.
- Если PSRAM недоступна (QEMU без octal-модели), `display_begin()` логирует
  предупреждение и отключает UI вместо `ESP_ERROR_CHECK`/ребута.

## Первый запуск и microSD

`settings_need_setup()` (core) проверяет Wi-Fi SSID и admin-пароль. Если
настройки неполные, прошивка поднимает LVGL/тач и показывает setup-экран
раньше captive portal:

```
setup screen
  ├─ Wi-Fi -> SoftAP portal (как раньше) -> save NVS -> reboot
  └─ microSD -> tablo-config.json -> validate -> save NVS -> reboot
             └─ нет файла -> tablo-config.example.json (один раз)
```

- Парсер и шаблон живут в `core/settings_json` и не зависят от Arduino.
- `hal_esp32/sd_card` — SPI-монтирование, чтение/запись, hot-plug-опрос.
- `hal_esp32/sd_config` — поиск и проверка `tablo-config.json`.
- `hal_esp32/setup_flow` — блокирующий мастер первого запуска.
- После настройки `sd_config` продолжает опрашивать слот в main loop. Новый
  валидный конфиг показывает подтверждение на экране; хэш применённого файла
  хранится в NVS, чтобы не спрашивать повторно.
- Все SD-операции выполняются в main loop (Core 1), не в render/net-задачах.
- В QEMU и rescue mode microSD не используется.

## Данные

`AppState` — POD-ish структура фиксированного размера: время, погода/AQI,
данные очистителя Xiaomi, пинги, OTA, Wi-Fi/IP, яркость, режим экрана
(`Day/Evening/Night`), расписание, версия, заметка, uptime, revision.
Никаких `std::string` в горячем обмене.

`IDataProvider` — блокирующий интерфейс, вызывается только из net-задачи.
Реализация на ESP32 планирует запросы по интервалам:

- NTP — при подключении Wi-Fi;
- погода/AQI — 15 мин;
- очиститель Xiaomi (miIO) — 5 мин, при ошибке backoff 30 с…5 мин;
- ICMP — 30 с;
- OTA manifest — при старте (через 15 с), раз в 6 ч и по кнопке.

Net-задача также раз в секунду пересчитывает режим экрана по локальному
времени. День/вечер/ночь задаются в `NightSchedule`; вечер и ночь
применяют отдельную палитру и масштабируют подсветку (60% и 25% от дневной).
WASM-провайдер читает `mock_data.json`, двигает часы и эмулирует OTA.
miIO-клиент (`hal_esp32/miio_client`) работает только в net-задаче: UDP 54321,
рукопожатие, AES-128-CBC/MD5, затем `get_properties` (MIoT) или `get_prop`
(старые 2/2S/Pro). Токен хранится в NVS и никогда не попадает в `AppState`.

## Настройки и секреты

`NvsSettings` (namespace `tablo`) читает defaults из `include/secrets.h`
(если есть), затем перекрывает значениями NVS. Настройки пишутся одним
NVS-blob (`SettingsBlob`, версия 3; v1/v2 мигрируют) вместе с хэшем последнего
microSD-конфига; старые по-ключевые значения читаются как fallback при
первом обновлении. Запись из UI дебаунсится
(`set_brightness`/`set_night_mode`/`set_schedule` + `tick()`), из `/config` —
сразу.

Секреты никогда не коммитятся: `include/secrets.h` в `.gitignore`,
`include/secrets.example.h` — только заглушки. В mock/docs — TEST-NET адреса.

## Web/OTA

- `/config` — HTML-форма, Basic Auth, сохранение в NVS + reboot.
- `/update` — multipart `.bin`, Basic Auth, пауза рендера, `Update.end(true)`.
- `/logs` — ring buffer 100×160 байт, Basic Auth.
- OTA GitHub Pages: manifest → semver compare → HTTPS bin → SHA-256 →
  `Update.end(true)`; при несовпадении `Update.abort()`.

## Rescue

`RTC_NOINIT_ATTR` хранит magic + число неудачных стартов. После трёх стартов
без 10 секунд стабильности:

- нет LVGL/API-воркеров;
- Wi-Fi + `/update` + `/logs`;
- счётчик очищается только успешным OTA или power-cycle RTC-памяти.

## Память

- RGB framebuffer: 480×480×2 ≈ 460 КБ в PSRAM;
- bounce buffer: 480×16×2 ≈ 15 КБ в PSRAM;
- LVGL heap: 192 КБ, выделяется из PSRAM;
- LVGL draw buffer: 480×16×2 ≈ 15 КБ internal DMA;
- net task stack 16 КБ, render 8 КБ.

При OOM сначала уменьшать `LV_MEM_SIZE`/draw buffer, не отключать bounce.
