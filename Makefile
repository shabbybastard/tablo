PIO ?= pio
PY ?= python3
UI_TEST_BUILD ?= build/ui-test

.PHONY: all fw wasm preview serve deps test test-ui test-fw test-all check clean

all: fw wasm

deps:
	$(PIO) pkg install -e esp32s3

fw:
	$(PIO) run -e esp32s3

wasm:
	scripts/build_wasm.sh

preview: wasm
	@echo "Preview: http://localhost:8088"
	$(PY) -m http.server 8088 --directory preview

serve:
	@echo "Preview: http://localhost:8088"
	$(PY) -m http.server 8088 --directory preview

test:
	$(MAKE) -C tests/host run

# Headless native LVGL smoke test: dashboard -> settings -> schedule -> setup -> OTA/SD modals.
test-ui:
	@test -f .pio/libdeps/esp32s3/lvgl/CMakeLists.txt || $(PIO) pkg install -e esp32s3
	cmake -S tests/ui -B $(UI_TEST_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build $(UI_TEST_BUILD)
	$(UI_TEST_BUILD)/dist/tablo_ui_test

# Real firmware in Espressif QEMU (esp32s3): boot, NVS, PSRAM, LVGL, net.
test-fw:
	scripts/test_qemu.sh

test-all: test test-ui test-fw

check:
	scripts/check_ui_portability.sh
	scripts/check_no_secrets.sh

clean:
	$(PIO) run -e esp32s3 -t clean
	rm -rf build/wasm build/ui-test preview/tablo.js preview/tablo.wasm preview/tablo.data
