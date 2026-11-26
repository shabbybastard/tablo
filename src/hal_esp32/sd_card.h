#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

/**
 * Hot-pluggable microSD card in SPI mode.
 *
 * The board has no card-detect GPIO, so insertion and removal are detected by
 * retrying the mount and probing the mounted filesystem. All calls run from
 * the Arduino main loop; they are not thread-safe and must not be used from
 * the render or network task.
 */
class SdCard {
 public:
  enum class State : uint8_t {
    Disabled,
    Absent,
    Mounted,
    Failed,
  };

  enum class IoResult : uint8_t {
    Ok,
    NotMounted,
    NotFound,
    TooLarge,
    Error,
  };

  bool begin(bool enabled = true);
  void end();
  void poll();

  void set_retry_interval(uint32_t ms) { retry_ms_ = ms; }

  State state() const { return state_; }
  bool mounted() const { return state_ == State::Mounted; }
  bool enabled() const { return enabled_; }

  bool exists(const char* path);
  IoResult read_file(const char* path, char* buffer, size_t cap, size_t& len);

  /** Write a new file without replacing an existing one. */
  bool write_file_if_missing(const char* path, const char* data, size_t len);

 private:
  bool try_mount();
  bool healthy();
  void unmount();

  State state_ = State::Disabled;
  bool enabled_ = false;
  bool spi_started_ = false;
  bool attempted_ = false;
  uint32_t retry_ms_ = 2000;
  uint32_t last_attempt_ms_ = 0;
  uint32_t last_health_ms_ = 0;
};

}  // namespace tablo
