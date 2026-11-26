#include "sd_card.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "board_pins.h"
#include "log_ring.h"
#include "qemu.h"

namespace tablo {
namespace {
constexpr const char* kMountPoint = "/sdcard";
}  // namespace

bool SdCard::begin(bool enabled) {
  enabled_ = enabled && !qemu_detected();
  if (!enabled_) {
    state_ = State::Disabled;
    if (qemu_detected()) {
      LOGI("sd: QEMU detected, microSD is not modeled");
    }
    return false;
  }

  // SD.begin() calls SPI.begin() internally. Initialize the bus explicitly
  // first so it uses the board pins instead of the SoC defaults.
  if (!spi_started_) {
    spi_started_ = SPI.begin(board::SD_SCK, board::SD_MISO, board::SD_MOSI,
                             board::SD_CS);
    if (!spi_started_) {
      LOGE("sd: SPI init failed (SCK=%d MISO=%d MOSI=%d CS=%d)", board::SD_SCK,
           board::SD_MISO, board::SD_MOSI, board::SD_CS);
      state_ = State::Failed;
      return false;
    }
    pinMode(board::SD_CS, OUTPUT);
    digitalWrite(board::SD_CS, HIGH);
  }
  state_ = State::Absent;
  attempted_ = false;
  probe_state_ = 0;
  mount_failures_ = 0;
  backoff_ms_ = retry_ms_;
  last_attempt_ms_ = 0;
  last_health_ms_ = 0;
  LOGI("sd: SPI ready (SCK=%d MISO=%d MOSI=%d CS=%d)", board::SD_SCK,
       board::SD_MISO, board::SD_MOSI, board::SD_CS);
  return true;
}

void SdCard::end() {
  unmount();
  enabled_ = false;
  state_ = State::Disabled;
}

void SdCard::poll() {
  if (!enabled_) {
    return;
  }
  if (state_ == State::Mounted) {
    if (millis() - last_health_ms_ < 1500U) {
      return;
    }
    last_health_ms_ = millis();
    if (!healthy()) {
      LOGW("sd: card removed or unreadable");
      unmount();
      state_ = State::Absent;
      attempted_ = false;
      probe_state_ = 0;
      mount_failures_ = 0;
      backoff_ms_ = retry_ms_;
      last_attempt_ms_ = millis();
    }
    return;
  }

  if (attempted_ && millis() - last_attempt_ms_ < backoff_ms_) {
    return;
  }
  attempted_ = true;
  last_attempt_ms_ = millis();

  // The board has no card-detect GPIO. A cheap CMD0 probe avoids running the
  // full SD.begin()/FatFs mount (and its noisy library logs) when the slot is
  // empty. If an inserted card answers, reinsertion resets any mount backoff.
  if (!card_responds()) {
    state_ = State::Absent;
    if (probe_state_ != 0) {
      probe_state_ = 0;
      mount_failures_ = 0;
      backoff_ms_ = retry_ms_;
    }
    return;
  }
  if (probe_state_ == 0) {
    probe_state_ = 1;
    mount_failures_ = 0;
    backoff_ms_ = retry_ms_;
  }

  if (try_mount()) {
    state_ = State::Mounted;
    mount_failures_ = 0;
    backoff_ms_ = retry_ms_;
    LOGI("sd: card mounted, %llu MB",
         static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  } else {
    state_ = State::Absent;
    if (mount_failures_ < 5) ++mount_failures_;
    uint32_t delay = retry_ms_;
    for (uint8_t i = 0; i < mount_failures_ && delay < 60000U; ++i) delay *= 2;
    backoff_ms_ = delay > 60000U ? 60000U : delay;
    LOGW("sd: mount failed, retry in %lu ms",
         static_cast<unsigned long>(backoff_ms_));
  }
}

bool SdCard::card_responds() {
  if (!spi_started_) {
    return false;
  }
  // CMD0 at 400 kHz: 0x01 means the card entered SPI idle state. With an empty
  // slot MISO stays high (0xFF), so SD.begin() is never called.
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(board::SD_CS, HIGH);
  for (int i = 0; i < 10; ++i) {
    SPI.transfer(0xFF);
  }
  digitalWrite(board::SD_CS, LOW);
  static const uint8_t kCmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
  for (uint8_t byte : kCmd0) {
    SPI.transfer(byte);
  }
  uint8_t response = 0xFF;
  for (int i = 0; i < 8 && response == 0xFF; ++i) {
    response = SPI.transfer(0xFF);
  }
  digitalWrite(board::SD_CS, HIGH);
  SPI.transfer(0xFF);
  SPI.endTransaction();
  return response == 0x01;
}

bool SdCard::try_mount() {
  if (!enabled_ || !spi_started_) {
    return false;
  }
  if (SD.begin(board::SD_CS, SPI, board::SD_SPI_HZ, kMountPoint, 5, false)) {
    if (SD.cardType() == CARD_NONE) {
      SD.end();
      return false;
    }
    return true;
  }
  return false;
}

bool SdCard::healthy() {
  if (!mounted()) {
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    return false;
  }
  // cardType() is cached and totalBytes() may hit only the cached FS state.
  // Read one raw sector as a real card probe; a removed card fails here.
  uint8_t sector[512] = {};
  return SD.readRAW(sector, 0);
}

void SdCard::unmount() {
  if (state_ == State::Mounted) {
    SD.end();
    LOGI("sd: card unmounted");
  }
}

bool SdCard::exists(const char* path) {
  if (!mounted() || path == nullptr) {
    return false;
  }
  // Use the VFS path directly. SD.exists() prepends the mountpoint, which
  // would double "/sdcard" for the absolute paths used by this class.
  struct stat info = {};
  return ::stat(path, &info) == 0;
}

SdCard::IoResult SdCard::read_file(const char* path, char* buffer, size_t cap,
                                   size_t& len) {
  len = 0;
  if (!mounted()) {
    return IoResult::NotMounted;
  }
  if (path == nullptr || buffer == nullptr || cap == 0) {
    return IoResult::Error;
  }

  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    return IoResult::NotFound;
  }
  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return IoResult::Error;
  }
  const long size = std::ftell(file);
  if (size < 0) {
    std::fclose(file);
    return IoResult::Error;
  }
  if (std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return IoResult::Error;
  }
  if (static_cast<size_t>(size) >= cap) {
    std::fclose(file);
    return IoResult::TooLarge;
  }

  const size_t read = std::fread(buffer, 1, static_cast<size_t>(size), file);
  const bool close_ok = std::fclose(file) == 0;
  if (read != static_cast<size_t>(size) || !close_ok) {
    return IoResult::Error;
  }
  buffer[read] = '\0';
  len = read;
  return IoResult::Ok;
}

bool SdCard::write_file_if_missing(const char* path, const char* data, size_t len) {
  if (!mounted() || path == nullptr || data == nullptr) {
    return false;
  }
  if (exists(path)) {
    return true;
  }

  const char* tmp = "/sdcard/.tablo-config.example.tmp";
  std::remove(tmp);

  FILE* file = std::fopen(tmp, "wb");
  if (file == nullptr) {
    return false;
  }
  const size_t written = std::fwrite(data, 1, len, file);
  const bool close_ok = std::fclose(file) == 0;
  if (written != len || !close_ok) {
    std::remove(tmp);
    return false;
  }
  if (std::rename(tmp, path) != 0) {
    std::remove(tmp);
    return false;
  }
  return true;
}

}  // namespace tablo
