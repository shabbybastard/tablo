CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra
CPPFLAGS += -I../../src -I../../include -I../../src/core
LDFLAGS += -pthread

BUILD_DIR := build
TARGET := $(BUILD_DIR)/test_core

SOURCES := \
	../../src/core/json.cpp \
	../../src/core/parsers.cpp \
	../../src/core/state_store.cpp \
	../../src/core/miio.cpp \
	../../src/core/settings_defaults.cpp \
	../../src/core/schedule.cpp \
	../../src/core/date_format.cpp \
	../../src/core/settings_json.cpp \
	../../src/core/settings_validate.cpp \
	test_core.cpp

HEADERS := \
	../../src/core/app_state.h \
	../../src/core/date_format.h \
	../../src/core/json.h \
	../../src/core/parsers.h \
	../../src/core/miio.h \
	../../src/core/schedule.h \
	../../src/core/settings.h \
	../../src/core/settings_json.h \
	../../src/core/settings_validate.h \
	../../src/core/state_store.h \
	../../src/core/version.h

.PHONY: run clean

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
