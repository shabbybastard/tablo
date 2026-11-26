# tablo

Автономный настольный информационный дисплей на **Guition ESP32-4848S040**:
ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB OPI PSRAM, 4″ квадратный IPS
480×480 (**ST7701S**, 16-bit RGB + 3-wire SPI init), ёмкостный тач **GT911**,
ШИМ-подсветка GPIO 38.

Интерфейс — LVGL 9.3 — собирается без изменений в двух таргетах:

- прошивка ESP32-S3 (`src/hal_esp32/`);
- WebAssembly-превью в браузере (`src/hal_wasm/` + SDL2).

## Возможности

- Тёмный дашборд 480×480: часы, погода/AQI, пинги GW/VPS/DNS,
  заметка и время работы.
- Видимая кнопка «Настройки» в верхней панели и отдельный экран расписания.
- Автоматический вечерний и ночной режим по локальному времени:
  отдельные палитры и приглушённая подсветка.
- OTA из GitHub Pages: `manifest.json`, SHA-256, кнопка «Обновить».
- Локальная загрузка прошивки через `/update`.
- `/config` с HTTP Basic Auth, `/logs` с ring buffer на 100 строк.
- Rescue mode после трёх неудачных стартов: только Wi-Fi, `/update`, `/logs`.
- NVS для всех настроек: переживают OTA.
- Первый запуск: SoftAP-портал для Wi-Fi или конфиг на microSD.
- microSD с горячей заменой: `tablo-config.json`, шаблон
  `tablo-config.example.json`.
- Версия в верхней панели краснеет и становится жирной при доступном OTA.
- WASM-превью с `mock_data.json` и автоперезагрузкой по изменению `tablo.wasm`.

## Железо и распиновка

| Сигнал | GPIO |
|---|---|
| LCD DE / VSYNC / HSYNC / PCLK | 18 / 17 / 16 / 21 |
| LCD R0–R4 | 11, 12, 13, 14, 0 |
| LCD G0–G5 | 8, 20, 3, 46, 9, 10 |
| LCD B0–B4 | 4, 5, 6, 7, 15 |
| ST7701 init: CS / SCK / MOSI | 39 / 48 / 47 |
| microSD: CS / MISO / MOSI / SCK | 42 / 41 / 47 / 48 |
| Подсветка | 38 (LEDC) |
| GT911: SDA / SCL | 19 / 45 |

PCLK 12 МГц, bounce buffer `480 × 16` px в PSRAM. Подробности и ссылки на
проверенные репозитории — в [docs/PINOUT.md](docs/PINOUT.md).

> GPIO 19/20 — это ещё и native USB ESP32-S3, но на этой плате они заняты
> тачем и G1. Native USB CDC отключён, отладка через UART.

## Архитектура

```
src/core/       AppState, Settings, StateStore, IDataProvider, IPlatform, JSON/parsers
src/ui/         чистый LVGL: dashboard, settings, расписание, modal, theme, шрифты
src/hal_esp32/  ST7701, GT911, LEDC, Wi-Fi portal, NVS, web, OTA, rescue, ping, main
src/hal_wasm/   Emscripten + SDL2, mock_data.json
preview/        index.html + mock_data.json
cmake/wasm/     CMake-сборка превью
scripts/        toolchain, сборка WASM, проверки, manifest
```

`src/ui` и `src/core` не включают Arduino/ESP-IDF. Потоки:

- **Core 1** — LVGL и тач (render task).
- **Core 0** — Wi-Fi, web-сервер, последовательные HTTP/HTTPS-запросы, OTA, ping.
- TLS-сессии не параллелятся; используется встроенный CA bundle.

Подробнее — [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Сборка

Требуются PlatformIO Core и Emscripten. В WSL2 они ставятся локально в `~`:

```bash
scripts/setup_toolchain.sh
source scripts/env.sh
pio --version
emcc --version
```

Скрипт ставит PlatformIO 6.2.0, Emscripten 4.0.23, Node LTS (Node нужен
только для однократной генерации шрифтов) и Espressif QEMU для `esp32s3`.

### Прошивка

```bash
pio run -e esp32s3
pio run -e esp32s3 -t upload       # плата должна быть доступна из WSL2
pio device monitor
```

Платформа: pioarduino 55.03.313 (Arduino core 3.3.13 / ESP-IDF 5.5.6).
Первая сборка скачивает toolchain (~2–3 ГБ).

### WASM-превью

```bash
make wasm                          # build/wasm -> preview/tablo.{js,wasm,data}
make preview                       # http://localhost:8088
# или:
python3 -m http.server 8088 --directory preview
```

`preview/index.html` автоматически перезагружается, когда меняется
`Last-Modified`/`Content-Length` у `preview/tablo.wasm`. Канвас 480×480,
тёмная рамка, мышь работает как тач. Слайдер яркости затемняет канвас через
CSS (`window.tabloSetBrightness`).

Если меняли `preview/mock_data.json`, пересоберите `make wasm` — файл
прелоадится в WASM-виртуальную ФС.

### Автотесты без железа

WASM-превью — интерактивный эмулятор для ручной проверки в браузере. Для
автоматических прогонов есть два независимых уровня:

```bash
make test-ui     # headless native LVGL: dashboard, settings, schedule, setup, OTA/SD
make test-fw     # реальная прошивка ESP32-S3 в Espressif QEMU
make test-all    # core + UI + firmware
```

`test-ui` собирает native-бинарь с тем же `src/ui`, `src/core` и
`preview/mock_data.json`, рендерит все экраны в память 480×480 и завершается
кодом `0` только при отсутствии ошибок отрисовки. Оконная система и SDL не
нужны.

`test-fw` собирает `pio run -e esp32s3`, склеивает
`bootloader.bin + partitions.bin + boot_app0.bin + firmware.bin` в
`.pio/build/esp32s3/qemu_flash.bin` (16 МБ) и запускает Espressif QEMU на
12 секунд. Проверяется UART: старт из `ota_0`, NVS, PSRAM 8 МБ, ST7701/LVGL,
отсутствие GT911 без зависания, offline-ветка Wi-Fi и старт сетевого воркера.
UART-лог сохраняется в `.pio/build/esp32s3/qemu_uart.log`.

QEMU ставится без sudo:

```bash
scripts/install_qemu.sh
qemu-system-xtensa -machine help | grep esp32s3
```

QEMU не моделирует Wi-Fi, lwIP, GT911 и microSD. Прошивка определяет эмулятор по
сигнатуре в APB_CTRL, пропускает блокирующий captive portal и остаётся
read-only, поэтому один и тот же `firmware.bin` грузится и в QEMU, и на
плате. Для octal PSRAM (N16R8) скрипт добавляет
`-global driver=ssi_psram,property=is_octal,value=true`; без этого QEMU
эмулирует quad PSRAM, и прошивка корректно отключает RGB-панель без ребута.

## Первый запуск

Если Wi-Fi ещё не настроен **или** пароль администратора всё ещё
`change-me`/пустой, на экране появляется выбор из двух вариантов.

### Вариант 1: Wi-Fi

1. Выберите «Настроить Wi-Fi». Плата поднимет защищённую WPA2-точку
   `tablo-XXXX` (канал 6); пароль виден на экране и в UART-логе.
2. Подключитесь к ней и откройте `http://192.168.4.1`.
3. Введите SSID/пароль Wi-Fi и admin user/password (минимум 8 символов).
   Если SSID не менялся, пароль Wi-Fi можно оставить пустым.
4. Устройство сохранит всё в NVS, перезагрузится и подключится к сети.
5. После подключения настройки доступны на `http://<ip>/config` или
   `http://tablo.local/config` (mDNS).

### Вариант 2: конфиг на microSD

1. Выберите «Конфиг с microSD» и вставьте карту FAT32. Если основного
   `tablo-config.json` нет, плата запишет шаблон
   `tablo-config.example.json`.
2. Извлеките карту, заполните шаблон на компьютере и переименуйте его в
   `tablo-config.json`.
3. Вставьте карту обратно — плата проверит файл, сохранит настройки в NVS и
   перезагрузится. Невалидный конфиг не меняет NVS.

Карта поддерживает горячую замену. На работающей плате новый
`tablo-config.json` вызывает запрос на подтверждение. Полная схема, имена
полей и ограничения — в [docs/SD_CONFIG.md](docs/SD_CONFIG.md). Помните, что
пароли на карте хранятся в открытом виде.

## Сервисные эндпоинты

| Метод | Путь | Назначение |
|---|---|---|
| GET/POST | `/config` | форма настроек, Basic Auth |
| GET/POST | `/update` | загрузка `.bin`, Basic Auth, пауза рендера |
| GET | `/logs` | последние 100 строк логов, Basic Auth |
| GET | `/update` | форма загрузки |

Логин/пароль задаются при первом запуске в captive portal (или в
`include/secrets.h`). Пароль `change-me` в примере — только placeholder:
пока он не изменён, устройство при старте откроет портал настройки.
**Смените пароль** через `/config`.

HTTP Basic Auth передаётся по локальной сети без шифрования; не выставляйте
устройство в интернет.

## OTA и релизы

Версия проекта берётся из `VERSION` — это единственный источник для локальной
сборки и CI. CI на push в `main`:

1. читает `VERSION` и собирает `pio run -e esp32s3` с этой `FW_VERSION`;
2. считает SHA-256;
3. публикует `firmware.bin`, OTA-манифест `manifest.json`, манифест
   веб-прошивальщика `flasher.json`, boot-образы и `index.html`
   через GitHub Pages Actions;
4. создаёт GitHub Release `v<версия>` и прикладывает к нему `firmware.bin`
   и `manifest.json`, чтобы прошивку можно было скачать со страницы релиза.

Чтобы выпустить новую версию: поднимите `VERSION`, добавьте раздел в
[CHANGELOG.md](CHANGELOG.md) и запушите в `main`.

Версия публикуется один раз на коммит: тег `v<версия>` создаётся на том
коммите, из которого собрана прошивка. Если тег уже существует и указывает на
другой коммит, CI остановится с ошибкой — сначала поднимите `VERSION`.
Повторный запуск workflow на том же коммите разрешён и обновляет файлы релиза;
если релиз был удалён, он будет создан заново на существующем теге.

Workflow запускается только при изменениях `VERSION`, `src/`, `include/`,
`cmake/`, `scripts/`, `platformio.ini`, `partitions.csv` или самого workflow,
поэтому правки документации не трогают OTA-канал.

Манифест:

```json
{
  "version": "1.0.3",
  "sha256": "…",
  "bin_url": "https://shabbybastard.github.io/tablo/firmware.bin",
  "size": 1234567,
  "released_at": "2026-11-26T00:00:00Z"
}
```

Устройство сравнивает версию с `FW_VERSION`, показывает модалку, скачивает
bin по HTTPS, проверяет SHA-256 и вызывает `Update.end(true)` только после
совпадения. При ошибке — `Update.abort()` и возврат UI.

**Включите GitHub Pages**: `Settings → Pages → Source: GitHub Actions`.
Без этого manifest будет отдавать 404.

## Прошивка по USB

На странице Pages есть веб-прошивальщик
[ESP Web Tools](https://github.com/esphome/esp-web-tools) (Apache-2.0):

1. откройте https://shabbybastard.github.io/tablo/ в Chrome, Edge или Opera
   на компьютере (или Chrome на Android);
2. подключите плату кабелем USB-C и нажмите «Прошить по USB»;
3. для чистой установки включите Erase — это стирает Wi-Fi и настройки;
4. если порт не определяется, зажмите BOOT и нажмите RESET.

`flasher.json` описывает четыре части: `bootloader.bin` (`0x0`),
`partitions.bin` (`0x8000`), `boot_app0.bin` (`0xe000`) и уже опубликованный
`firmware.bin` (`0x10000`). Отдельный `factory.bin` не нужен: OTA и USB
используют один и тот же app-образ, а boot-образы занимают ~31 КБ.

Firefox и Safari Web Serial не поддерживают.

## Rescue mode

Счётчик неудачных стартов хранится в `RTC_NOINIT_ATTR`. После трёх стартов
без 10 секунд стабильной работы включается rescue:

- не запускаются LVGL и API-воркеры;
- поднимаются только Wi-Fi, `/update`, `/logs`;
- если сохранённая сеть недоступна, rescue сам поднимает WPA2-портал
  `tablo-XXXX` (пароль на экране/в UART-логе);
- счётчик не сбрасывается, пока прошивка не будет успешно обновлена;
- успешный `/update` очищает счётчик и перезагружает плату.

Счётчик сбрасывается через 10 секунд нормальной работы.

## Настройки

NVS namespace `tablo`: настройки хранятся одним blob-объектом (старые
по-ключевые значения читаются как fallback при обновлении), плюс хэш
последнего применённого microSD-конфига. Секреты в репозиторий не попадают:
`include/secrets.h` в `.gitignore`, `include/secrets.example.h` содержит
только заглушки.

Источники данных (best-effort, всё опционально):

- **Погода/AQI** — Open-Meteo, без API-ключа; нужны широта/долгота.
- **ICMP** — ESP-IDF `esp_ping` до GW/VPS/DNS.

Если поле не заполнено, UI показывает `--`, ошибки не блокируют загрузку.

### Расписание экрана

В настройках есть отдельный экран «Расписание»:

- день / вечер / ночь по локальному времени (по умолчанию 07:00 / 18:00 / 23:00);
- вечерняя палитра и 60% дневной яркости;
- ночная палитра и 25% дневной яркости;
- авторежим по умолчанию выключен;
- при включённом авто ручной переключатель ночного режима блокируется.
- расписание также доступно в `/config` и в JSON-конфиге microSD.

## Диагностика

- **Горизонтальные сдвиги/рябь** — проверить питание и шлейф, затем PCLK
  (12 → 10 МГц) и `LCD_BOUNCE_PX`; не менять порядок RGB наугад.
- **Тач не работает** — проверить адрес 0x5D/0x14, SDA19/SCL45, питание;
  драйвер только polling, INT/RST на плате не разведены.
- **OTA 404** — не включён GitHub Pages или не совпал `bin_url`.
- **TLS ошибки** — проверьте NTP: сертификаты не проходят при неверном времени.
- **Сборка WASM не находит LVGL** — выполните `pio pkg install -e esp32s3`
  или `make wasm` (скрипт сделает это сам).
- **WSL2 не видит плату** — настройте `usbipd-win`; для сборки это не нужно.

## Версионирование

- `VERSION` — единственный источник версии (сейчас `1.0.3`).
- Локальная сборка и CI читают `VERSION`; `FW_VERSION` можно переопределить
  вручную, но в CI версия всегда совпадает с `VERSION`.
- GitHub Release и тег `v<версия>` создаёт CI из `VERSION`.
- История изменений — в [CHANGELOG.md](CHANGELOG.md).

## Ограничения MVP

- Нет мониторинга произвольных сервисов.
- Плата не тестировалась на реальном железе в этой среде: проверены сборка,
  линковка и WASM-артефакты. Настройка PCLK/bounce может потребовать подстройки.

## Лицензия

MIT, см. [LICENSE](LICENSE). Init-последовательность ST7701 взята из MIT-проекта
aquaElectronics/esp32-4848s040-st7701 с атрибуцией в исходнике.

Веб-прошивальщик использует [ESP Web Tools](https://github.com/esphome/esp-web-tools)
под лицензией Apache-2.0; библиотека подключается с CDN и не входит в репозиторий.
