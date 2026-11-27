#include "miio.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "json.h"

namespace tablo {
namespace {

const MiioMapping kMiotLegacy = {
    "zhimi 3/3H/Pro H", false,
    2, 2,   // power
    2, 5,   // mode
    2, 4,   // fan level
    3, 7,   // humidity
    3, 8,   // temperature
    3, 6,   // PM2.5 / AQI
    4, 3,   // filter life
    0, 0,   // TVOC
};

const MiioMapping kMiotMb4 = {
    "zhimi 3C", false,
    2, 1,   // power
    2, 4,   // mode
    0, 0,   // fan level
    0, 0,   // humidity
    0, 0,   // temperature
    3, 4,   // PM2.5
    4, 1,   // filter life
    0, 0,   // TVOC
};

const MiioMapping kMiotVa2 = {
    "zhimi 4/4 Pro", false,
    2, 1,   // power
    2, 4,   // mode
    2, 5,   // fan level
    3, 1,   // humidity
    3, 7,   // temperature
    3, 4,   // PM2.5
    4, 1,   // filter life
    0, 0,   // TVOC
};

const MiioMapping kMiotRma = {
    "zhimi 4 Lite", false,
    2, 1,   // power
    2, 4,   // mode
    0, 0,   // fan level
    3, 1,   // humidity
    3, 7,   // temperature
    3, 4,   // PM2.5
    4, 1,   // filter life
    0, 0,   // TVOC
};

const MiioMapping kMiotZa1 = {
    "smartmi", false,
    2, 1,   // power
    2, 5,   // mode
    2, 4,   // fan level
    3, 7,   // humidity
    3, 8,   // temperature
    3, 6,   // PM2.5
    4, 3,   // filter life
    3, 1,   // TVOC
};

const MiioMapping kLegacyProp = {
    "zhimi 2/2S/Pro", true,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool equals(const char* a, const char* b) {
  return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
}

bool starts_with(const char* text, const char* prefix) {
  return text != nullptr && prefix != nullptr &&
         std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

bool is_legacy_model(const char* model) {
  static const char* const kExact[] = {
      "zhimi.airpurifier.v1", "zhimi.airpurifier.v2",
      "zhimi.airpurifier.v3", "zhimi.airpurifier.v4",
      "zhimi.airpurifier.v5", "zhimi.airpurifier.v6",
      "zhimi.airpurifier.v7", "zhimi.airpurifier.m1",
      "zhimi.airpurifier.m2", "zhimi.airpurifier.ma1",
      "zhimi.airpurifier.ma2", "zhimi.airpurifier.sa1",
      "zhimi.airpurifier.sa2", "zhimi.airpurifier.mc1",
      "zhimi.airpurifier.mc2",
  };
  for (const char* candidate : kExact) {
    if (equals(model, candidate)) return true;
  }
  return false;
}

bool is_known_miot_model(const char* model) {
  static const char* const kExact[] = {
      "zhimi.airpurifier.ma4", "zhimi.airpurifier.mb3",
      "zhimi.airpurifier.mb3a", "zhimi.airp.mb3a",
      "zhimi.airpurifier.va1", "zhimi.airpurifier.vb2",
      "zhimi.airpurifier.mb4", "zhimi.airp.mb4",
      "zhimi.airp.mb4a", "zhimi.airp.mb5",
      "zhimi.airp.mb5a", "zhimi.airp.va2",
      "zhimi.airp.vb4", "zhimi.airp.rmb1",
      "zhimi.airpurifier.rma1", "zhimi.airpurifier.rma2",
      "zhimi.airpurifier.rma3", "zhimi.airpurifier.za1",
      "zhimi.airp.meb1", "zhimi.airp.cpa4",
  };
  for (const char* candidate : kExact) {
    if (equals(model, candidate)) return true;
  }
  return false;
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int clamp_int(long value, int low, int high) {
  if (value < low) return low;
  if (value > high) return high;
  return static_cast<int>(value);
}

bool appendf(char* out, size_t cap, size_t& used, const char* format, ...) {
  if (out == nullptr || cap == 0 || used >= cap) return false;
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(out + used, cap - used, format, args);
  va_end(args);
  if (written < 0 || static_cast<size_t>(written) >= cap - used) return false;
  used += static_cast<size_t>(written);
  return true;
}

bool append_property(char* out, size_t cap, size_t& used, bool& first,
                     const char* did, uint8_t siid, uint8_t piid) {
  if (siid == 0 || piid == 0) return true;
  if (!appendf(out, cap, used,
               "%s{\"did\":\"%s\",\"siid\":%u,\"piid\":%u}",
               first ? "" : ",", did, static_cast<unsigned>(siid),
               static_cast<unsigned>(piid))) {
    return false;
  }
  first = false;
  return true;
}

int mode_from_legacy_text(const char* text) {
  if (text == nullptr) return -1;
  if (std::strcmp(text, "auto") == 0) return 0;
  if (std::strcmp(text, "silent") == 0) return 1;
  if (std::strcmp(text, "favorite") == 0) return 2;
  if (std::strcmp(text, "medium") == 0 ||
      std::strcmp(text, "high") == 0 ||
      std::strcmp(text, "strong") == 0 ||
      std::strcmp(text, "low") == 0) {
    return 3;
  }
  if (std::strcmp(text, "idle") == 0) return 4;
  return -1;
}

}  // namespace

bool miio_parse_token(const char* text, uint8_t out[kMiioTokenBytes]) {
  if (text == nullptr || out == nullptr) return false;
  if (std::strlen(text) != kMiioTokenHexChars) return false;
  for (size_t i = 0; i < kMiioTokenBytes; ++i) {
    const int high = hex_value(text[i * 2]);
    const int low = hex_value(text[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    out[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

bool miio_valid_ipv4(const char* text) {
  if (text == nullptr || text[0] == '\0') return false;
  const char* p = text;
  for (int part = 0; part < 4; ++part) {
    if (*p < '0' || *p > '9') return false;
    int value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + (*p - '0');
      if (value > 255) return false;
      ++digits;
      if (digits > 3) return false;
      ++p;
    }
    if (part < 3) {
      if (*p != '.') return false;
      ++p;
    }
  }
  return *p == '\0';
}

const MiioMapping* miio_mapping_for_model(const char* model) {
  if (model == nullptr || model[0] == '\0') return &kMiotVa2;

  if (equals(model, "zhimi.airpurifier.ma4") ||
      equals(model, "zhimi.airpurifier.mb3") ||
      equals(model, "zhimi.airpurifier.mb3a") ||
      equals(model, "zhimi.airp.mb3a") ||
      equals(model, "zhimi.airpurifier.va1") ||
      equals(model, "zhimi.airpurifier.vb2")) {
    return &kMiotLegacy;
  }

  if (equals(model, "zhimi.airpurifier.mb4") ||
      equals(model, "zhimi.airp.mb4") || equals(model, "zhimi.airp.mb4a")) {
    return &kMiotMb4;
  }

  if (equals(model, "zhimi.airp.rmb1") ||
      equals(model, "zhimi.airpurifier.rma1") ||
      equals(model, "zhimi.airpurifier.rma2") ||
      equals(model, "zhimi.airpurifier.rma3") ||
      starts_with(model, "zhimi.airp.rma")) {
    return &kMiotRma;
  }

  if (equals(model, "zhimi.airpurifier.za1")) return &kMiotZa1;

  if (equals(model, "zhimi.airp.mb5") ||
      equals(model, "zhimi.airp.mb5a") ||
      equals(model, "zhimi.airp.va2") ||
      equals(model, "zhimi.airp.vb4") ||
      equals(model, "zhimi.airp.meb1") ||
      equals(model, "zhimi.airp.cpa4")) {
    return &kMiotVa2;
  }

  if (is_legacy_model(model) || starts_with(model, "zhimi.airpurifier.v")) {
    return &kLegacyProp;
  }

  /* Unknown zhimi.airp.* devices are usually close to the 4/4 Pro layout.
   * The poller logs the model so a new mapping can be added later. */
  return &kMiotVa2;
}

bool miio_model_supported(const char* model) {
  return is_known_miot_model(model) || is_legacy_model(model) ||
         starts_with(model, "zhimi.airp.rma");
}

size_t miio_build_get_properties(uint16_t id, const MiioMapping& mapping,
                                 char* out, size_t cap) {
  if (out == nullptr || cap == 0) return 0;
  size_t used = 0;
  if (!appendf(out, cap, used,
               "{\"id\":%u,\"method\":\"get_properties\",\"params\":[",
               static_cast<unsigned>(id))) {
    return 0;
  }
  bool first = true;
  if (!append_property(out, cap, used, first, "power", mapping.power_siid,
                       mapping.power_piid) ||
      !append_property(out, cap, used, first, "mode", mapping.mode_siid,
                       mapping.mode_piid) ||
      !append_property(out, cap, used, first, "fan_level", mapping.fan_siid,
                       mapping.fan_piid) ||
      !append_property(out, cap, used, first, "humidity",
                       mapping.humidity_siid, mapping.humidity_piid) ||
      !append_property(out, cap, used, first, "temperature",
                       mapping.temperature_siid, mapping.temperature_piid) ||
      !append_property(out, cap, used, first, "aqi", mapping.aqi_siid,
                       mapping.aqi_piid) ||
      !append_property(out, cap, used, first, "filter_life",
                       mapping.filter_siid, mapping.filter_piid) ||
      !append_property(out, cap, used, first, "tvoc", mapping.tvoc_siid,
                       mapping.tvoc_piid)) {
    return 0;
  }
  if (!appendf(out, cap, used, "]}")) return 0;
  return used;
}

size_t miio_build_get_prop(uint16_t id, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return 0;
  const int written = std::snprintf(
      out, cap,
      "{\"id\":%u,\"method\":\"get_prop\",\"params\":[\"power\",\"mode\","
      "\"aqi\",\"humidity\",\"temp_dec\",\"filter1_life\"]}",
      static_cast<unsigned>(id));
  if (written < 0 || static_cast<size_t>(written) >= cap) return 0;
  return static_cast<size_t>(written);
}

bool miio_parse_model(const char* json, size_t len, char* out, size_t cap) {
  if (json == nullptr || out == nullptr || cap == 0) return false;
  out[0] = '\0';
  JsonDoc doc;
  if (!doc.parse(json, len)) return false;
  if (!doc.get_string("result.model", out, cap)) return false;
  return out[0] != '\0';
}

bool miio_parse_miot_properties(const char* json, size_t len,
                                MiioReading& out) {
  out = MiioReading{};
  if (json == nullptr) return false;
  JsonDoc doc;
  if (!doc.parse(json, len)) return false;
  const int count = doc.array_size("result");
  if (count <= 0) return false;

  bool matched = false;
  for (int i = 0; i < count; ++i) {
    char did[24] = {};
    if (!doc.get_string_at("result", i, "did", did, sizeof(did))) continue;

    long code = 0;
    if (doc.get_int_at("result", i, "code", code) && code != 0) continue;

    if (std::strcmp(did, "power") == 0) {
      bool value = false;
      if (doc.get_bool_at("result", i, "value", value)) {
        out.power = value;
        out.has_power = true;
        matched = true;
      } else {
        long numeric = 0;
        if (doc.get_int_at("result", i, "value", numeric)) {
          out.power = numeric != 0;
          out.has_power = true;
          matched = true;
        }
      }
    } else if (std::strcmp(did, "mode") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.mode = static_cast<int16_t>(clamp_int(value, 0, 15));
        matched = true;
      }
    } else if (std::strcmp(did, "fan_level") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.fan_level = static_cast<int16_t>(clamp_int(value, 0, 15));
        matched = true;
      }
    } else if (std::strcmp(did, "humidity") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.humidity = static_cast<int16_t>(clamp_int(value, 0, 100));
        out.has_humidity = true;
        matched = true;
      }
    } else if (std::strcmp(did, "temperature") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.temperature = static_cast<int16_t>(clamp_int(value, -40, 80));
        out.has_temperature = true;
        matched = true;
      }
    } else if (std::strcmp(did, "aqi") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.pm25 = static_cast<int16_t>(clamp_int(value, 0, 1000));
        matched = true;
      }
    } else if (std::strcmp(did, "filter_life") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.filter_life = static_cast<int16_t>(clamp_int(value, 0, 100));
        matched = true;
      }
    } else if (std::strcmp(did, "tvoc") == 0) {
      long value = 0;
      if (doc.get_int_at("result", i, "value", value)) {
        out.tvoc = static_cast<int16_t>(clamp_int(value, 0, 10000));
        matched = true;
      }
    }
  }
  return matched;
}

bool miio_parse_legacy_props(const char* json, size_t len, MiioReading& out) {
  out = MiioReading{};
  if (json == nullptr) return false;
  JsonDoc doc;
  if (!doc.parse(json, len)) return false;
  const int count = doc.array_size("result");
  if (count <= 0) return false;

  bool matched = false;

  char text[32] = {};
  if (doc.get_string_at("result", 0, nullptr, text, sizeof(text))) {
    out.power = std::strcmp(text, "on") == 0;
    out.has_power = true;
    matched = true;
  }
  text[0] = '\0';
  if (doc.get_string_at("result", 1, nullptr, text, sizeof(text))) {
    out.mode = static_cast<int16_t>(mode_from_legacy_text(text));
    matched = true;
  }

  long value = 0;
  if (doc.get_int_at("result", 2, nullptr, value)) {
    out.pm25 = static_cast<int16_t>(clamp_int(value, 0, 1000));
    matched = true;
  }
  if (doc.get_int_at("result", 3, nullptr, value)) {
    out.humidity = static_cast<int16_t>(clamp_int(value, 0, 100));
    out.has_humidity = true;
    matched = true;
  }
  if (doc.get_int_at("result", 4, nullptr, value)) {
    const long rounded = value >= 0 ? (value + 5) / 10 : (value - 5) / 10;
    out.temperature = static_cast<int16_t>(clamp_int(rounded, -40, 80));
    out.has_temperature = true;
    matched = true;
  }
  if (doc.get_int_at("result", 5, nullptr, value)) {
    out.filter_life = static_cast<int16_t>(clamp_int(value, 0, 100));
    matched = true;
  }
  return matched;
}

const char* miio_mode_text(int mode) {
  switch (mode) {
    case 0:
      return "Авто";
    case 1:
      return "Тихий";
    case 2:
      return "Избранный";
    case 3:
      return "Вентиляция";
    case 4:
      return "Ожидание";
    default:
      return "Режим";
  }
}

const char* miio_quality_text(int pm25) {
  if (pm25 < 0) return "Нет данных";
  if (pm25 <= 15) return "Чисто";
  if (pm25 <= 35) return "Норма";
  if (pm25 <= 55) return "Умеренно";
  if (pm25 <= 150) return "Плохо";
  return "Опасно";
}

}  // namespace tablo
