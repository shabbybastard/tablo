#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

/**
 * Small DOM-less JSON reader built on jsmn. Paths use dots for objects
 * ("weather.temp_c"); arrays are addressed by name plus an index.
 */
class JsonDoc {
 public:
  JsonDoc();
  ~JsonDoc();
  JsonDoc(const JsonDoc&) = delete;
  JsonDoc& operator=(const JsonDoc&) = delete;

  bool parse(const char* json, size_t len);

  bool has(const char* path) const;

  bool get_string(const char* path, char* out, size_t cap) const;

  /**
   * Decoded UTF-8 length of a string value, without copying it. Returns -1
   * when the path does not exist or the value is not a string.
   */
  int get_string_length(const char* path) const;

  bool get_int(const char* path, long& out) const;

  /** Like get_int(), but rejects fractional/exponent notation. */
  bool get_int_exact(const char* path, long& out) const;

  bool get_uint(const char* path, uint32_t& out) const;
  bool get_bool(const char* path, bool& out) const;

  int array_size(const char* path) const;

  bool get_string_at(const char* array_path, int index, const char* field, char* out,
                     size_t cap) const;
  bool get_int_at(const char* array_path, int index, const char* field, long& out) const;
  bool get_bool_at(const char* array_path, int index, const char* field, bool& out) const;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace tablo
