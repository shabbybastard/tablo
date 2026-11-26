#include "setup_flow.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "boot_guard.h"
#include "core/platform.h"
#include "core/settings_json.h"
#include "core/state_store.h"
#include "log_ring.h"
#include "platform_esp32.h"
#include "sd_config.h"

namespace tablo {
namespace {

void set_text(char* dst, size_t cap, const char* text) {
  if (dst == nullptr || cap == 0) {
    return;
  }
  std::snprintf(dst, cap, "%s", text != nullptr ? text : "");
}

void publish(const SetupInfo& info) { g_state.update_setup(info); }

void show_choice(SetupInfo& info, const SdConfigService& sd) {
  info.mode = SetupMode::Choice;
  info.card_present = sd.mounted();
  info.card_mounted = sd.mounted();
  if (sd.mounted()) {
    if (sd.config_ready()) {
      set_text(info.status, sizeof(info.status),
               "На карте найден tablo-config.json. Выберите microSD, чтобы применить.");
    } else if (sd.config_invalid()) {
      set_text(info.status, sizeof(info.status),
               "На карте найден конфиг с ошибкой. Выберите microSD, чтобы увидеть детали.");
    } else {
      set_text(info.status, sizeof(info.status),
               "Карта подключена. Выберите microSD, чтобы проверить файлы.");
    }
  } else {
    set_text(info.status, sizeof(info.status),
             "Вставьте microSD с tablo-config.json или настройте Wi-Fi с телефона.");
  }
  set_text(info.hint, sizeof(info.hint),
           "Карту можно вставлять и извлекать без выключения платы.");
}

void show_sd_waiting(SetupInfo& info) {
  info.mode = SetupMode::Sd;
  info.card_present = false;
  info.card_mounted = false;
  set_text(info.status, sizeof(info.status),
           "Вставьте microSD (FAT32). Карта определится автоматически.");
  set_text(info.hint, sizeof(info.hint),
           "Если основного конфига нет, плата запишет tablo-config.example.json. "
           "Заполните его и переименуйте в tablo-config.json.");
}

void show_sd_mounted(SetupInfo& info, bool wrote_example) {
  info.card_present = true;
  info.card_mounted = true;
  if (wrote_example) {
    set_text(info.status, sizeof(info.status),
             "Шаблон tablo-config.example.json готов на карте.");
    set_text(info.hint, sizeof(info.hint),
             "Извлеките карту, заполните шаблон на компьютере, переименуйте в "
             "tablo-config.json и вставьте обратно.");
  } else {
    set_text(info.status, sizeof(info.status),
             "Не удалось создать шаблон. Проверьте FAT32 и защиту от записи.");
    set_text(info.hint, sizeof(info.hint),
             "Можно создать tablo-config.json на компьютере и вставить карту снова.");
  }
}

void show_sd_error(SetupInfo& info, const char* error) {
  if (error != nullptr && error[0] != '\0') {
    std::snprintf(info.status, sizeof(info.status), "Ошибка конфига: %.150s", error);
  } else {
    set_text(info.status, sizeof(info.status), "Конфиг на карте не прошёл проверку.");
  }
  set_text(info.hint, sizeof(info.hint),
           "Исправьте файл tablo-config.json на компьютере, извлеките и вставьте "
           "карту снова. Настройки в NVS не изменены.");
}

bool apply_config(NvsSettings& settings, SdConfigService& sd, SetupInfo& info,
                  const Settings& next, uint32_t hash) {
  set_text(info.status, sizeof(info.status),
           "Конфиг проверен. Сохраняем настройки...");
  publish(info);
  delay(700);

  if (!settings.save_checked(next)) {
    LOGE("sd: failed to save settings to NVS");
    set_text(info.status, sizeof(info.status),
             "Не удалось сохранить настройки в NVS. Изменения не применены.");
    set_text(info.hint, sizeof(info.hint),
             "Извлеките и вставьте карту, чтобы повторить попытку.");
    publish(info);
    return false;
  }
  if (!sd.remember_applied(hash)) {
    LOGW("sd: config saved, but hash was not persisted");
  }
  boot_guard_mark_stable();
  LOGI("sd: config applied, ssid='%s', hash=%08lx", next.wifi_ssid,
       static_cast<unsigned long>(hash));

  set_text(info.status, sizeof(info.status), "Настройки применены. Перезагрузка...");
  set_text(info.hint, sizeof(info.hint), "");
  publish(info);
  sd.end();
  delay(1200);
  ESP.restart();
  while (true) {
    delay(1000);
  }
}

}  // namespace

void run_setup_flow(NvsSettings& settings, WifiManager& wifi) {
  LOGI("setup: first-run wizard started");
  // Heap: the service embeds Settings/Event buffers and this loop never
  // returns; keeping it off the Arduino loop-task stack is intentional.
  auto* sd = new SdConfigService(settings);
  sd->begin(true);

  SetupInfo info;
  info.active = true;
  show_choice(info, *sd);
  publish(info);

  const uint32_t started_ms = millis();
  bool stable_marked = false;
  bool was_mounted = false;
  bool example_checked = false;
  bool error_shown = false;
  bool apply_attempted = false;

  while (true) {
    if (!stable_marked && millis() - started_ms > 10000U) {
      boot_guard_mark_stable();
      stable_marked = true;
    }

    const uint32_t actions = platform_take_actions();
    const bool choose_wifi =
        (actions & (1u << static_cast<uint32_t>(PlatformAction::SetupChooseWifi))) != 0;
    const bool choose_sd =
        (actions & (1u << static_cast<uint32_t>(PlatformAction::SetupChooseSd))) != 0;
    const bool back =
        (actions & (1u << static_cast<uint32_t>(PlatformAction::SetupBack))) != 0;

    if (choose_wifi) {
      LOGI("setup: Wi-Fi chosen");
      info.mode = SetupMode::Wifi;
      info.card_present = sd->mounted();
      info.card_mounted = sd->mounted();
      set_text(info.ap_ssid, sizeof(info.ap_ssid), wifi.ap_name());
      set_text(info.ap_password, sizeof(info.ap_password), wifi.ap_password());
      set_text(info.status, sizeof(info.status),
               "Точка доступа поднимается. Подключитесь к ней с телефона.");
      set_text(info.hint, sizeof(info.hint),
               "Откройте http://192.168.4.1. После сохранения плата перезагрузится.");
      publish(info);
      sd->end();
      boot_guard_mark_stable();
      stable_marked = true;
      wifi.begin(true, false);

      // begin() returns only when the portal could not be started (a
      // successful save reboots the board).
      LOGW("setup: Wi-Fi portal did not start, returning to choice");
      info.mode = SetupMode::Choice;
      set_text(info.status, sizeof(info.status),
               "Не удалось поднять точку доступа. Попробуйте ещё раз или используйте microSD.");
      set_text(info.hint, sizeof(info.hint),
               "Карту можно вставлять и извлекать без выключения платы.");
      publish(info);
      sd->begin(true);
      was_mounted = false;
      example_checked = false;
      error_shown = false;
      apply_attempted = false;
      continue;
    }

    if (choose_sd) {
      LOGI("setup: microSD chosen");
      show_sd_waiting(info);
      publish(info);
      was_mounted = sd->mounted();
      example_checked = false;
      error_shown = false;
      apply_attempted = false;
    }

    if (back && info.mode == SetupMode::Sd) {
      LOGI("setup: back to choice");
      show_choice(info, *sd);
      publish(info);
      was_mounted = sd->mounted();
      example_checked = false;
      error_shown = false;
      apply_attempted = false;
    }

    sd->poll();
    const bool now_mounted = sd->mounted();

    if (now_mounted != was_mounted) {
      LOGI("setup: sd mounted=%d", now_mounted ? 1 : 0);
      was_mounted = now_mounted;
      example_checked = false;
      error_shown = false;
      apply_attempted = false;
      info.card_present = now_mounted;
      info.card_mounted = now_mounted;

      if (info.mode == SetupMode::Choice) {
        show_choice(info, *sd);
        publish(info);
      } else if (info.mode == SetupMode::Sd) {
        if (!now_mounted) {
          set_text(info.status, sizeof(info.status),
                   "Карта извлечена. Вставьте её обратно после редактирования.");
          set_text(info.hint, sizeof(info.hint),
                   "Настройки в NVS не изменены. Горячая замена поддерживается.");
          publish(info);
        }
      }
    }

    if (info.mode == SetupMode::Sd && now_mounted) {
      if (sd->config_ready()) {
        if (!apply_attempted) {
          apply_attempted = true;
          apply_config(settings, *sd, info, sd->parsed_settings(),
                       sd->parsed_hash());
        }
      } else if (sd->config_invalid()) {
        if (!error_shown) {
          show_sd_error(info, sd->error());
          publish(info);
          error_shown = true;
        }
      } else if (!example_checked) {
        const bool wrote = sd->write_example_if_missing();
        show_sd_mounted(info, wrote);
        publish(info);
        example_checked = true;
      }
    }

    delay(20);
  }
}

}  // namespace tablo
