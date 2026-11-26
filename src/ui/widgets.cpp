#include "widgets.h"

#include <cstdio>
#include <cstring>

#include "theme.h"

namespace tablo {
namespace ui {

lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_style_t* style) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text != nullptr ? text : "");
  lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
  if (style != nullptr) {
    lv_obj_add_style(label, style, 0);
  }
  remove_flags(label, LV_OBJ_FLAG_SCROLLABLE);
  return label;
}

void set_label_text(lv_obj_t* label, const char* text) {
  if (label == nullptr) {
    return;
  }
  const char* next = text != nullptr ? text : "";
  const char* current = lv_label_get_text(label);
  if (current == nullptr || std::strcmp(current, next) != 0) {
    lv_label_set_text(label, next);
  }
}

void sanitize_text(const char* in, char* out, size_t out_size) {
  if (out == nullptr || out_size == 0) {
    return;
  }
  if (in == nullptr) {
    in = "";
  }
  const auto* p = reinterpret_cast<const unsigned char*>(in);
  size_t used = 0;
  const auto put = [&](char c) {
    if (used + 1 < out_size) {
      out[used++] = c;
    }
  };

  while (*p != 0 && used + 1 < out_size) {
    /* The generated glyph set intentionally stays small: normalise a few
     * common typographic characters that APIs may return into ASCII. */
    if (p[0] == 0xE2 && p[1] == 0x80) {
      if (p[2] == 0xA6) {  /* … */
        put('.');
        put('.');
        put('.');
        p += 3;
        continue;
      }
      if (p[2] == 0x93 || p[2] == 0x94) {  /* – — */
        put('-');
        p += 3;
        continue;
      }
      if (p[2] == 0x98 || p[2] == 0x99) {  /* ‘ ’ */
        put('\'');
        p += 3;
        continue;
      }
      if (p[2] == 0x9C || p[2] == 0x9D) {  /* “ ” */
        put('"');
        p += 3;
        continue;
      }
    }
    if (p[0] == 0xC2 && p[1] == 0xA0) {  /* non-breaking space */
      put(' ');
      p += 2;
      continue;
    }
    if (p[0] == 0xC2 && (p[1] == 0xAB || p[1] == 0xBB)) {  /* « » */
      put('"');
      p += 2;
      continue;
    }
    put(static_cast<char>(*p));
    ++p;
  }
  out[used] = '\0';
}

void set_label_text_safe(lv_obj_t* label, const char* text) {
  char buffer[192];
  sanitize_text(text, buffer, sizeof(buffer));
  set_label_text(label, buffer);
}

lv_obj_t* make_card(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(card, &theme().card, 0);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  remove_flags(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
  return card;
}

lv_obj_t* make_divider(lv_obj_t* parent, int32_t x, int32_t y, int32_t w) {
  lv_obj_t* line = lv_obj_create(parent);
  lv_obj_remove_style_all(line);
  lv_obj_add_flag(line, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(line, &theme().divider, 0);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_size(line, w, 1);
  remove_flags(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return line;
}

lv_obj_t* make_dot(lv_obj_t* parent, int32_t x, int32_t y, int32_t size,
                   lv_color_t color) {
  lv_obj_t* dot = lv_obj_create(parent);
  lv_obj_remove_style_all(dot);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_pos(dot, x, y);
  lv_obj_set_size(dot, size, size);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dot, color, 0);
  remove_flags(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return dot;
}

void set_pad_all(lv_obj_t* obj, int32_t pad) {
  lv_obj_set_style_pad_left(obj, pad, 0);
  lv_obj_set_style_pad_right(obj, pad, 0);
  lv_obj_set_style_pad_top(obj, pad, 0);
  lv_obj_set_style_pad_bottom(obj, pad, 0);
}

void make_non_interactive(lv_obj_t* obj) {
  if (obj == nullptr) {
    return;
  }
  remove_flags(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void remove_flags(lv_obj_t* obj, uint32_t flags) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(flags));
}

void format_uptime(uint32_t seconds, char* out, size_t out_size) {
  if (out == nullptr || out_size == 0) {
    return;
  }
  const uint32_t days = seconds / 86400U;
  const uint32_t hours = (seconds % 86400U) / 3600U;
  const uint32_t minutes = (seconds % 3600U) / 60U;
  if (days > 0) {
    std::snprintf(out, out_size, "%u д %u ч",
                  static_cast<unsigned>(days),
                  static_cast<unsigned>(hours));
  } else if (hours > 0) {
    std::snprintf(out, out_size, "%u ч %02u мин",
                  static_cast<unsigned>(hours),
                  static_cast<unsigned>(minutes));
  } else {
    std::snprintf(out, out_size, "%u мин", static_cast<unsigned>(minutes));
  }
}

lv_color_t aqi_color(int aqi) {
  if (aqi < 0) {
    return palette().muted;
  }
  if (aqi <= 50) {
    return palette().green;
  }
  if (aqi <= 100) {
    return palette().amber;
  }
  if (aqi <= 150) {
    return lv_color_hex(0xFB923C);
  }
  if (aqi <= 200) {
    return palette().red;
  }
  return lv_color_hex(0xA78BFA);
}

const char* aqi_text(int aqi) {
  if (aqi < 0) {
    return "нет данных";
  }
  if (aqi <= 50) {
    return "хорошо";
  }
  if (aqi <= 100) {
    return "умеренно";
  }
  if (aqi <= 150) {
    return "вредно";
  }
  if (aqi <= 200) {
    return "плохо";
  }
  return "опасно";
}

}  // namespace ui
}  // namespace tablo
