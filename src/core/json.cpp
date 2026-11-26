#include "json.h"

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#define JSMN_STATIC
#define JSMN_STRICT
#include "third_party/jsmn/jsmn.h"

namespace tablo {
namespace {

constexpr size_t kInitialTokens = 512;
constexpr size_t kMaxTokens = 4096;
constexpr size_t kNumberBuffer = 64;

bool read_hex4(const char*& cursor, const char* end, uint32_t& value) {
  if (end - cursor < 4) {
    return false;
  }
  uint32_t result = 0;
  for (int i = 0; i < 4; ++i) {
    const char c = cursor[i];
    int digit = -1;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      digit = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      digit = c - 'A' + 10;
    }
    if (digit < 0) {
      return false;
    }
    result = (result << 4) | static_cast<uint32_t>(digit);
  }
  cursor += 4;
  value = result;
  return true;
}

bool is_json_number(const char* text) {
  const char* p = text;
  if (*p == '-') {
    ++p;
  }
  if (*p < '0' || *p > '9') {
    return false;
  }
  while (*p >= '0' && *p <= '9') {
    ++p;
  }
  if (*p == '.') {
    ++p;
    if (*p < '0' || *p > '9') {
      return false;
    }
    while (*p >= '0' && *p <= '9') {
      ++p;
    }
  }
  if (*p == 'e' || *p == 'E') {
    ++p;
    if (*p == '+' || *p == '-') {
      ++p;
    }
    if (*p < '0' || *p > '9') {
      return false;
    }
    while (*p >= '0' && *p <= '9') {
      ++p;
    }
  }
  return *p == '\0';
}

bool is_uint_text(const char* text) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') {
      return false;
    }
  }
  return true;
}

}  // namespace

struct JsonDoc::Impl {
  const char* text = nullptr;
  size_t length = 0;
  jsmntok_t* tokens = nullptr;
  int count = 0;
  bool ok = false;

  ~Impl() { std::free(tokens); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl() = default;

  const jsmntok_t* token(int index) const {
    if (index < 0 || index >= count) {
      return nullptr;
    }
    return &tokens[index];
  }

  int resolve(const char* path) const {
    if (!ok || path == nullptr || count <= 0) {
      return -1;
    }
    if (*path == '\0') {
      return 0;
    }

    int current = 0;
    const char* segment = path;
    while (*segment != '\0') {
      const jsmntok_t* object = token(current);
      if (object == nullptr || object->type != JSMN_OBJECT) {
        return -1;
      }

      const char* dot = std::strchr(segment, '.');
      const size_t length = dot != nullptr ? static_cast<size_t>(dot - segment)
                                           : std::strlen(segment);
      if (length == 0) {
        return -1;
      }

      const int value = find_key(current, segment, length);
      if (value < 0) {
        return -1;
      }
      current = value;

      if (dot == nullptr) {
        return current;
      }
      segment = dot + 1;
    }
    return -1;
  }

  /**
   * Index of the first token after `index` and all of its descendants.
   * Container tokens store their logical child count (object pairs / array
   * elements), not the raw token span, so character boundaries are used.
   */
  int skip_token(int index) const {
    const jsmntok_t* tok = token(index);
    if (tok == nullptr) {
      return -1;
    }
    int next = index + 1;
    while (next < count && tokens[next].start < tok->end) {
      ++next;
    }
    return next;
  }

  int find_key(int object_index, const char* key, size_t key_length) const {
    const jsmntok_t* object = token(object_index);
    if (object == nullptr || object->type != JSMN_OBJECT) {
      return -1;
    }

    int index = object_index + 1;
    for (int pair = 0; pair < object->size; ++pair) {
      const jsmntok_t* key_token = token(index);
      if (key_token == nullptr || key_token->type != JSMN_STRING) {
        return -1;
      }
      const int value = skip_token(index);
      if (token(value) == nullptr) {
        return -1;
      }

      const size_t token_length = static_cast<size_t>(key_token->end - key_token->start);
      if (token_length == key_length &&
          std::memcmp(text + key_token->start, key, key_length) == 0) {
        return value;
      }
      index = skip_token(value);
      if (index < 0) {
        return -1;
      }
    }
    return -1;
  }

  int array_element(const char* array_path, int index, const char* field) const {
    if (index < 0) {
      return -1;
    }
    const int array_index = resolve(array_path);
    const jsmntok_t* array = token(array_index);
    if (array == nullptr || array->type != JSMN_ARRAY || index >= array->size) {
      return -1;
    }

    int element = array_index + 1;
    for (int i = 0; i < index; ++i) {
      element = skip_token(element);
      if (element < 0) {
        return -1;
      }
    }
    if (token(element) == nullptr) {
      return -1;
    }

    if (field == nullptr) {
      return element;
    }
    return find_key(element, field, std::strlen(field));
  }

  /**
   * Decodes a JSON string. When `out` is null only the decoded byte length is
   * computed, which lets callers reject too-long values instead of silently
   * truncating them.
   */
  bool decode_string(const jsmntok_t& tok, char* out, size_t cap,
                     size_t& decoded) const {
    decoded = 0;
    if (tok.type != JSMN_STRING || tok.start < 0 || tok.end < tok.start ||
        static_cast<size_t>(tok.end) > length) {
      return false;
    }
    if (out != nullptr && cap == 0) {
      return false;
    }

    const char* p = text + tok.start;
    const char* const end = text + tok.end;

    auto emit_byte = [&](unsigned char value) {
      if (out != nullptr && decoded + 1 < cap) {
        out[decoded] = static_cast<char>(value);
      }
      ++decoded;
    };
    auto emit_codepoint = [&](uint32_t code_point) {
      if (code_point > 0x10FFFF ||
          (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        code_point = 0xFFFD;
      }
      if (code_point < 0x80) {
        emit_byte(static_cast<unsigned char>(code_point));
      } else if (code_point < 0x800) {
        emit_byte(static_cast<unsigned char>(0xC0 | (code_point >> 6)));
        emit_byte(static_cast<unsigned char>(0x80 | (code_point & 0x3F)));
      } else if (code_point < 0x10000) {
        emit_byte(static_cast<unsigned char>(0xE0 | (code_point >> 12)));
        emit_byte(static_cast<unsigned char>(0x80 | ((code_point >> 6) & 0x3F)));
        emit_byte(static_cast<unsigned char>(0x80 | (code_point & 0x3F)));
      } else {
        emit_byte(static_cast<unsigned char>(0xF0 | (code_point >> 18)));
        emit_byte(static_cast<unsigned char>(0x80 | ((code_point >> 12) & 0x3F)));
        emit_byte(static_cast<unsigned char>(0x80 | ((code_point >> 6) & 0x3F)));
        emit_byte(static_cast<unsigned char>(0x80 | (code_point & 0x3F)));
      }
    };

    while (p < end) {
      const unsigned char c = static_cast<unsigned char>(*p++);
      if (c != '\\') {
        emit_byte(c);
        continue;
      }
      if (p >= end) {
        return false;
      }

      const char escaped = *p++;
      switch (escaped) {
        case '"':
          emit_byte('"');
          break;
        case '\\':
          emit_byte('\\');
          break;
        case '/':
          emit_byte('/');
          break;
        case 'b':
          emit_byte('\b');
          break;
        case 'f':
          emit_byte('\f');
          break;
        case 'n':
          emit_byte('\n');
          break;
        case 'r':
          emit_byte('\r');
          break;
        case 't':
          emit_byte('\t');
          break;
        case 'u': {
          uint32_t code_point = 0;
          if (!read_hex4(p, end, code_point)) {
            return false;
          }
          if (code_point >= 0xD800 && code_point <= 0xDBFF) {
            const char* const after_high = p;
            uint32_t low = 0;
            if (end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
              p += 2;
              if (read_hex4(p, end, low) && low >= 0xDC00 && low <= 0xDFFF) {
                code_point =
                    0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
              } else {
                p = after_high;
                code_point = 0xFFFD;
              }
            } else {
              code_point = 0xFFFD;
            }
          } else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
            code_point = 0xFFFD;
          }
          emit_codepoint(code_point);
          break;
        }
        default:
          return false;
      }
    }

    if (out != nullptr) {
      out[decoded < cap ? decoded : cap - 1] = '\0';
    }
    return true;
  }

  bool read_string(const jsmntok_t& tok, char* out, size_t cap) const {
    if (out == nullptr) {
      return false;
    }
    size_t decoded = 0;
    return decode_string(tok, out, cap, decoded);
  }

  bool read_long_exact(const jsmntok_t& tok, long& out) const {
    if (tok.type != JSMN_PRIMITIVE) {
      return false;
    }
    const int token_length = tok.end - tok.start;
    if (token_length <= 0 || static_cast<size_t>(token_length) >= kNumberBuffer) {
      return false;
    }

    char buffer[kNumberBuffer];
    std::memcpy(buffer, text + tok.start, static_cast<size_t>(token_length));
    buffer[token_length] = '\0';

    const char* p = buffer;
    if (*p == '-') {
      ++p;
    }
    if (*p < '0' || *p > '9') {
      return false;
    }
    while (*p >= '0' && *p <= '9') {
      ++p;
    }
    if (*p != '\0') {
      return false;
    }

    errno = 0;
    char* parse_end = nullptr;
    const long value = std::strtol(buffer, &parse_end, 10);
    if (errno == ERANGE || parse_end == buffer || *parse_end != '\0') {
      return false;
    }
    out = value;
    return true;
  }

  bool read_long(const jsmntok_t& tok, long& out) const {
    if (tok.type != JSMN_PRIMITIVE) {
      return false;
    }
    const int token_length = tok.end - tok.start;
    if (token_length <= 0 || static_cast<size_t>(token_length) >= kNumberBuffer) {
      return false;
    }

    char buffer[kNumberBuffer];
    std::memcpy(buffer, text + tok.start, static_cast<size_t>(token_length));
    buffer[token_length] = '\0';
    if (!is_json_number(buffer)) {
      return false;
    }

    errno = 0;
    char* parse_end = nullptr;
    const double value = std::strtod(buffer, &parse_end);
    if (errno == ERANGE || parse_end == buffer || *parse_end != '\0' ||
        !std::isfinite(value)) {
      return false;
    }

    const double rounded = std::round(value);
    if (rounded < static_cast<double>(std::numeric_limits<long>::min()) ||
        rounded > static_cast<double>(std::numeric_limits<long>::max())) {
      return false;
    }
    out = static_cast<long>(rounded);
    return true;
  }

  bool read_uint(const jsmntok_t& tok, uint32_t& out) const {
    if (tok.type != JSMN_PRIMITIVE) {
      return false;
    }
    const int token_length = tok.end - tok.start;
    if (token_length <= 0 || static_cast<size_t>(token_length) >= 32) {
      return false;
    }

    char buffer[32];
    std::memcpy(buffer, text + tok.start, static_cast<size_t>(token_length));
    buffer[token_length] = '\0';
    if (!is_uint_text(buffer)) {
      return false;
    }

    errno = 0;
    char* parse_end = nullptr;
    const unsigned long long value = std::strtoull(buffer, &parse_end, 10);
    if (errno == ERANGE || parse_end == buffer || *parse_end != '\0' ||
        value > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
  }

  bool read_bool(const jsmntok_t& tok, bool& out) const {
    if (tok.type != JSMN_PRIMITIVE) {
      return false;
    }
    const int token_length = tok.end - tok.start;
    if (token_length == 4 && std::memcmp(text + tok.start, "true", 4) == 0) {
      out = true;
      return true;
    }
    if (token_length == 5 && std::memcmp(text + tok.start, "false", 5) == 0) {
      out = false;
      return true;
    }
    return false;
  }
};

JsonDoc::JsonDoc() : impl_(new (std::nothrow) Impl()) {}

JsonDoc::~JsonDoc() { delete impl_; }

bool JsonDoc::parse(const char* json, size_t len) {
  if (impl_ == nullptr) {
    return false;
  }

  impl_->ok = false;
  impl_->text = nullptr;
  impl_->length = 0;
  impl_->count = 0;

  if (json == nullptr || len == 0 || len > static_cast<size_t>(INT_MAX)) {
    return false;
  }

  size_t capacity = kInitialTokens;
  int result = JSMN_ERROR_NOMEM;
  for (;;) {
    jsmntok_t* tokens = static_cast<jsmntok_t*>(
        std::realloc(impl_->tokens, capacity * sizeof(jsmntok_t)));
    if (tokens == nullptr) {
      return false;
    }
    impl_->tokens = tokens;

    jsmn_parser parser;
    jsmn_init(&parser);
    result = jsmn_parse(&parser, json, len, impl_->tokens,
                        static_cast<unsigned int>(capacity));
    if (result != JSMN_ERROR_NOMEM || capacity >= kMaxTokens) {
      break;
    }
    capacity = (capacity * 2 > kMaxTokens) ? kMaxTokens : capacity * 2;
  }

  if (result <= 0) {
    return false;
  }

  impl_->text = json;
  impl_->length = len;
  impl_->count = result;
  impl_->ok = true;
  return true;
}

bool JsonDoc::has(const char* path) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  return impl_->resolve(path) >= 0;
}

bool JsonDoc::get_string(const char* path, char* out, size_t cap) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  return tok != nullptr && impl_->read_string(*tok, out, cap);
}

int JsonDoc::get_string_length(const char* path) const {
  if (impl_ == nullptr || !impl_->ok) {
    return -1;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  if (tok == nullptr) {
    return -1;
  }
  size_t decoded = 0;
  if (!impl_->decode_string(*tok, nullptr, 0, decoded) ||
      decoded > static_cast<size_t>(INT_MAX)) {
    return -1;
  }
  return static_cast<int>(decoded);
}

bool JsonDoc::get_int(const char* path, long& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  return tok != nullptr && impl_->read_long(*tok, out);
}

bool JsonDoc::get_int_exact(const char* path, long& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  return tok != nullptr && impl_->read_long_exact(*tok, out);
}

bool JsonDoc::get_uint(const char* path, uint32_t& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  return tok != nullptr && impl_->read_uint(*tok, out);
}

bool JsonDoc::get_bool(const char* path, bool& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  return tok != nullptr && impl_->read_bool(*tok, out);
}

int JsonDoc::array_size(const char* path) const {
  if (impl_ == nullptr || !impl_->ok) {
    return -1;
  }
  const jsmntok_t* tok = impl_->token(impl_->resolve(path));
  if (tok == nullptr || tok->type != JSMN_ARRAY) {
    return -1;
  }
  return tok->size;
}

bool JsonDoc::get_string_at(const char* array_path, int index, const char* field, char* out,
                            size_t cap) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->array_element(array_path, index, field));
  return tok != nullptr && impl_->read_string(*tok, out, cap);
}

bool JsonDoc::get_int_at(const char* array_path, int index, const char* field,
                         long& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->array_element(array_path, index, field));
  return tok != nullptr && impl_->read_long(*tok, out);
}

bool JsonDoc::get_bool_at(const char* array_path, int index, const char* field,
                          bool& out) const {
  if (impl_ == nullptr || !impl_->ok) {
    return false;
  }
  const jsmntok_t* tok = impl_->token(impl_->array_element(array_path, index, field));
  return tok != nullptr && impl_->read_bool(*tok, out);
}

}  // namespace tablo
