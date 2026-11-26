#include "setup_screen.h"

#include <cstdio>

#include "core/platform.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

lv_obj_t* make_choice_button(lv_obj_t* parent, int32_t y, const char* title,
                             const char* subtitle, lv_event_cb_t callback) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_add_style(button, &theme().button_ghost, 0);
  lv_obj_add_style(button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_pos(button, 12, y);
  lv_obj_set_size(button, 456, 88);
  lv_obj_set_style_shadow_width(button, 0, 0);
  remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(button, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* title_label = make_label(button, title, nullptr);
  lv_obj_set_style_text_font(title_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(title_label, palette().text, 0);
  lv_obj_set_pos(title_label, 18, 16);
  lv_obj_set_size(title_label, 390, 26);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* subtitle_label = make_label(button, subtitle, nullptr);
  lv_obj_set_style_text_font(subtitle_label, &ui_font_14, 0);
  lv_obj_set_style_text_color(subtitle_label, palette().muted, 0);
  lv_obj_set_pos(subtitle_label, 18, 48);
  lv_obj_set_size(subtitle_label, 400, 22);
  lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* arrow = make_label(button, ">", nullptr);
  lv_obj_set_style_text_font(arrow, &ui_font_28, 0);
  lv_obj_set_style_text_color(arrow, palette().accent, 0);
  lv_obj_set_pos(arrow, 414, 22);
  lv_obj_set_size(arrow, 28, 32);
  lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_RIGHT, 0);

  return button;
}

void set_hidden(lv_obj_t* object, bool hidden) {
  if (object == nullptr) {
    return;
  }
  if (hidden) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else {
    remove_flags(object, LV_OBJ_FLAG_HIDDEN);
  }
}

void choose_wifi_cb(lv_event_t*) {
  platform().requestAction(PlatformAction::SetupChooseWifi);
}

void choose_sd_cb(lv_event_t*) {
  platform().requestAction(PlatformAction::SetupChooseSd);
}

void back_cb(lv_event_t*) {
  platform().requestAction(PlatformAction::SetupBack);
}

}  // namespace

void setup_screen_create(SetupScreen& s) {
  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_remove_style_all(screen);
  lv_obj_add_style(screen, &theme().screen, 0);
  remove_flags(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
  s.screen = screen;

  s.brand = make_label(screen, "tablo", &theme().accent18);
  lv_obj_set_pos(s.brand, 14, 10);

  s.version = make_label(screen, "", &theme().muted14);
  lv_obj_set_pos(s.version, 376, 12);
  lv_obj_set_size(s.version, 90, 20);
  lv_obj_set_style_text_align(s.version, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(s.version, LV_LABEL_LONG_MODE_DOTS);

  s.back_button = lv_button_create(screen);
  lv_obj_remove_style_all(s.back_button);
  lv_obj_add_style(s.back_button, &theme().button_ghost, 0);
  lv_obj_add_style(s.back_button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_pos(s.back_button, 352, 6);
  lv_obj_set_size(s.back_button, 114, 32);
  lv_obj_set_style_shadow_width(s.back_button, 0, 0);
  remove_flags(s.back_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(s.back_button, back_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* back_label = make_label(s.back_button, "Назад", &theme().muted14);
  lv_obj_set_size(back_label, 108, 20);
  lv_obj_set_style_text_align(back_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(back_label);

  s.title = make_label(screen, "Первая настройка", &theme().text28);
  lv_obj_set_pos(s.title, 14, 44);
  lv_obj_set_size(s.title, 452, 38);
  lv_label_set_long_mode(s.title, LV_LABEL_LONG_MODE_DOTS);

  s.subtitle = make_label(screen, "Выберите способ подключения", &theme().muted14);
  lv_obj_set_pos(s.subtitle, 14, 84);
  lv_obj_set_size(s.subtitle, 452, 20);
  lv_label_set_long_mode(s.subtitle, LV_LABEL_LONG_MODE_DOTS);

  s.wifi_button = make_choice_button(screen, 118, "Настроить Wi-Fi",
                                      "Точка доступа и настройка с телефона",
                                      choose_wifi_cb);
  s.sd_button = make_choice_button(screen, 218, "Конфиг с microSD",
                                    "Вставьте карту с tablo-config.json",
                                    choose_sd_cb);

  s.info_card = make_card(screen, 12, 118, 456, 190);
  set_hidden(s.info_card, true);

  s.info_title = make_label(s.info_card, "microSD", &theme().text18);
  lv_obj_set_pos(s.info_title, 16, 14);
  lv_obj_set_size(s.info_title, 424, 24);

  s.info_value = make_label(s.info_card, "", &theme().accent28);
  lv_obj_set_pos(s.info_value, 16, 42);
  lv_obj_set_size(s.info_value, 424, 36);
  lv_label_set_long_mode(s.info_value, LV_LABEL_LONG_MODE_DOTS);

  s.info_line1 = make_label(s.info_card, "", &theme().muted14);
  lv_obj_set_pos(s.info_line1, 16, 88);
  lv_obj_set_size(s.info_line1, 424, 40);
  lv_label_set_long_mode(s.info_line1, LV_LABEL_LONG_MODE_WRAP);

  s.info_line2 = make_label(s.info_card, "", &theme().muted14);
  lv_obj_set_pos(s.info_line2, 16, 132);
  lv_obj_set_size(s.info_line2, 424, 40);
  lv_label_set_long_mode(s.info_line2, LV_LABEL_LONG_MODE_WRAP);

  s.status = make_label(screen, "", &theme().text14);
  lv_obj_set_pos(s.status, 14, 326);
  lv_obj_set_size(s.status, 452, 66);
  lv_label_set_long_mode(s.status, LV_LABEL_LONG_MODE_WRAP);

  s.hint = make_label(screen, "", &theme().muted14);
  lv_obj_set_pos(s.hint, 14, 402);
  lv_obj_set_size(s.hint, 452, 62);
  lv_label_set_long_mode(s.hint, LV_LABEL_LONG_MODE_WRAP);
}

void setup_screen_update(SetupScreen& s, const AppState& state) {
  if (s.screen == nullptr) {
    return;
  }
  const SetupInfo& setup = state.setup;

  char version[32];
  if (state.fw_version[0] != '\0') {
    std::snprintf(version, sizeof(version), "v%s", state.fw_version);
  } else {
    std::snprintf(version, sizeof(version), "v-");
  }
  set_label_text_safe(s.version, version);

  const bool choice = setup.mode == SetupMode::Choice || setup.mode == SetupMode::None;
  const bool wifi = setup.mode == SetupMode::Wifi;
  const bool sd = setup.mode == SetupMode::Sd;

  set_hidden(s.wifi_button, !choice);
  set_hidden(s.sd_button, !choice);
  set_hidden(s.info_card, choice);
  set_hidden(s.back_button, !sd);
  set_hidden(s.version, sd);

  if (choice) {
    set_label_text(s.title, "Первая настройка");
    set_label_text(s.subtitle, "Выберите способ подключения");
  } else if (wifi) {
    set_label_text(s.title, "Настройка Wi-Fi");
    set_label_text(s.subtitle, "Подключитесь к точке доступа платы");
  } else {
    set_label_text(s.title, "Конфиг с microSD");
    set_label_text(s.subtitle, "Горячая замена карты поддерживается");
  }

  if (wifi) {
    set_label_text(s.info_title, "Точка доступа");
    set_label_text_safe(s.info_value,
                        setup.ap_ssid[0] != '\0' ? setup.ap_ssid : "tablo-XXXX");
    char line[96];
    std::snprintf(line, sizeof(line), "Пароль: %s",
                  setup.ap_password[0] != '\0' ? setup.ap_password : "на экране/в UART");
    set_label_text_safe(s.info_line1, line);
    set_label_text(s.info_line2,
                   "Откройте http://192.168.4.1 и сохраните настройки.");
  } else if (sd) {
    set_label_text(s.info_title, "Карта памяти");
    if (setup.card_mounted) {
      set_label_text(s.info_value, "Карта подключена");
      set_label_text(s.info_line1, "Основной файл: tablo-config.json");
      set_label_text(s.info_line2,
                     "Если конфига нет, плата запишет tablo-config.example.json.");
    } else if (setup.card_present) {
      set_label_text(s.info_value, "Карта не распознана");
      set_label_text(s.info_line1, "Нужна карта FAT32 и исправный слот.");
      set_label_text(s.info_line2,
                     "Извлеките и вставьте карту снова — без перезагрузки.");
    } else {
      set_label_text(s.info_value, "Карта не найдена");
      set_label_text(s.info_line1, "Вставьте microSD с tablo-config.json.");
      set_label_text(s.info_line2,
                     "Можно вставить пустую карту: плата запишет шаблон.");
    }
  }

  if (setup.status[0] != '\0') {
    set_label_text_safe(s.status, setup.status);
  } else if (choice) {
    set_label_text(s.status,
                   "Wi-Fi настраивается с телефона. Конфиг с карты применяется "
                   "автоматически после проверки.");
  } else {
    set_label_text(s.status, "");
  }

  if (setup.hint[0] != '\0') {
    set_label_text_safe(s.hint, setup.hint);
  } else if (choice) {
    set_label_text(s.hint, "После сохранения настроек плата перезагрузится.");
  } else if (sd) {
    set_label_text(s.hint,
                   "Карту можно извлекать и вставлять на горячую. После "
                   "заполнения шаблона переименуйте его в tablo-config.json.");
  } else {
    set_label_text(s.hint, "Не выключайте питание во время сохранения.");
  }
}

}  // namespace ui
}  // namespace tablo
