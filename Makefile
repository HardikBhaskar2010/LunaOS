# Mahina OS — Root Makefile
# Authority: Volume VII / 01_implementation_roadmap.md §0.1 Build Toolchain
# Toolchain: Clang (Volume VI / 01_coding_standards.md)
# Standards: C17, -Wall -Wextra -Werror (zero-warning policy)
#
# Targets:
#   make all              — Build luna-init, luna-init-ctl, and image
#   make luna-init        — Build only luna-init
#   make luna-init-ctl    — Build only luna-init-ctl
#   make image            — Build disk image (requires Linux/WSL2)
#   make run-qemu         — Launch QEMU with Mahina image
#   make clean            — Remove all build artifacts
#   make test-unit        — Run unit tests with ASan + UBSan
#   make test-fuzz        — Run TOML fuzz regression (5 seconds)
#   make lint             — Run clang-tidy on all sources

# Probe for libdrm (required by lgp-compositor)
LIBDRM_HEADERS_EXIST := $(shell if [ -f /usr/include/libdrm/xf86drm.h ] || [ -f /usr/include/xf86drm.h ] || pkg-config --exists libdrm 2>/dev/null; then echo "yes"; else echo "no"; fi)
ifeq ($(LIBDRM_HEADERS_EXIST),no)
  $(warning "WARNING: libdrm headers not found. Building lgp-compositor will fail. Please install libdrm-dev or equivalent.")
endif

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------

CC      := clang
LD      := ld.lld
AR      := llvm-ar
STRIP   := llvm-strip

# ---------------------------------------------------------------------------
# Version
# ---------------------------------------------------------------------------

MAHINA_VERSION := 0.1.0
MAHINA_CODENAME := Waxing
WALLPAPER_MP4 ?= retro-lake-at-night.1920x1080.mp4

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------

SRC_DIR        := src
BUILD_DIR      := build
TESTS_DIR      := tests
SCRIPTS_DIR    := scripts

LUNA_INIT_SRC  := $(SRC_DIR)/luna-init
LUNA_CTL_SRC   := $(SRC_DIR)/luna-init-ctl
LUNA_SPLASH_SRC := $(SRC_DIR)/luna-splash
BUILD_INIT     := $(BUILD_DIR)/luna-init
BUILD_CTL      := $(BUILD_DIR)/luna-init-ctl
BUILD_SPLASH   := $(BUILD_DIR)/luna-splash

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

# Base flags — strictly compliant with Volume VI / 01_coding_standards.md
CFLAGS := \
    -std=c17 \
    -Wall \
    -Wextra \
    -Werror \
    -Wpedantic \
    -Wstrict-prototypes \
    -Wmissing-prototypes \
    -Wcast-align \
    -Wformat=2 \
    -Wnull-dereference \
    -O2 \
    -fstack-protector-strong \
    -D_GNU_SOURCE \
    -DMAHINA_VERSION=\"$(MAHINA_VERSION)\" \
    -DMAHINA_CODENAME=\"$(MAHINA_CODENAME)\"

# Static binary flags for luna-init (must be statically linked — 04_init_system.md)
CFLAGS_STATIC := $(CFLAGS) -static

# Debug / test flags (AddressSanitizer + UBSan — mandatory in all test builds)
# Authority: Volume VI / 01_coding_standards.md §5, Volume VI / 05_testing_standards.md
CFLAGS_TEST := \
    $(CFLAGS) \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -fno-sanitize-recover=all \
    -g \
    -O1

# Fuzz flags (AFL++ integration)
CFLAGS_FUZZ := \
    $(CFLAGS) \
    -fsanitize=fuzzer,address \
    -g

# Include paths
INCLUDES := -I$(SRC_DIR)

# ---------------------------------------------------------------------------
# luna-init sources
# ---------------------------------------------------------------------------

LUNA_INIT_SOURCES := \
    $(LUNA_INIT_SRC)/log.c \
    $(LUNA_INIT_SRC)/toml.c \
    $(LUNA_INIT_SRC)/mount.c \
    $(LUNA_INIT_SRC)/signal.c \
    $(LUNA_INIT_SRC)/reaper.c \
    $(LUNA_INIT_SRC)/service.c \
    $(LUNA_INIT_SRC)/depgraph.c \
    $(LUNA_INIT_SRC)/supervisor.c \
    $(LUNA_INIT_SRC)/shutdown.c \
    $(LUNA_INIT_SRC)/hostname.c \
    $(LUNA_INIT_SRC)/panic.c \
    $(LUNA_INIT_SRC)/console.c \
    $(LUNA_INIT_SRC)/ctl.c \
    $(LUNA_INIT_SRC)/splash.c \
    $(LUNA_INIT_SRC)/main.c

LUNA_INIT_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_INIT_SOURCES))

# ---------------------------------------------------------------------------
# luna-splash sources
# ---------------------------------------------------------------------------

LUNA_SPLASH_SOURCES := \
    $(LUNA_SPLASH_SRC)/render.c \
    $(LUNA_SPLASH_SRC)/ipc.c \
    $(LUNA_SPLASH_SRC)/main.c

LUNA_SPLASH_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_SPLASH_SOURCES))

# ---------------------------------------------------------------------------
# luna-init-ctl sources
# ---------------------------------------------------------------------------

LUNA_CTL_SOURCES := \
    $(LUNA_CTL_SRC)/main.c

LUNA_CTL_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_CTL_SOURCES))

# ---------------------------------------------------------------------------
# Unit test sources
# ---------------------------------------------------------------------------

UNITY_SRC    := $(TESTS_DIR)/vendor/unity/unity.c
UNITY_OBJ    := $(BUILD_DIR)/tests/vendor/unity.o

LUNA_INIT_TEST_SOURCES := \
    $(TESTS_DIR)/unit/luna-init/log_test.c \
    $(TESTS_DIR)/unit/luna-init/toml_test.c \
    $(TESTS_DIR)/unit/luna-init/depgraph_test.c \
    $(TESTS_DIR)/unit/luna-init/service_test.c

LGP_UNIT_TEST_SOURCES := \
    $(TESTS_DIR)/unit/lgp-compositor/protocol_test.c

LGP_UNIT_TEST_SUPPORT := \
    $(SRC_DIR)/lgp-compositor/protocol/tlv.c \
    $(SRC_DIR)/lgp-compositor/protocol/surface.c \
    $(SRC_DIR)/lgp-compositor/protocol/wm.c \
    $(SRC_DIR)/lgp-compositor/scene/surface.c \
    $(SRC_DIR)/lgp-compositor/logging/log.c

LUNA_GUI_UNIT_TEST_SOURCES := \
    $(TESTS_DIR)/unit/luna-gui/gui_test.c

LUNA_GUI_UNIT_TEST_SUPPORT := \
    $(SRC_DIR)/luna-gui/core/application.c \
    $(SRC_DIR)/luna-gui/core/window.c \
    $(SRC_DIR)/luna-gui/core/keyboard.c \
    $(SRC_DIR)/luna-gui/render/canvas.c \
    $(SRC_DIR)/luna-gui/render/font.c \
    $(SRC_DIR)/luna-gui/render/render.c \
    $(SRC_DIR)/luna-gui/widgets/widget.c \
    $(SRC_DIR)/luna-gui/widgets/button.c \
    $(SRC_DIR)/luna-gui/widgets/label.c \
    $(SRC_DIR)/luna-gui/widgets/vbox.c \
    $(SRC_DIR)/luna-gui/widgets/hbox.c \
    $(SRC_DIR)/luna-gui/widgets/canvas_widget.c \
    $(SRC_DIR)/luna-gui/widgets/scroll.c

# ---------------------------------------------------------------------------
# Primary targets
# ---------------------------------------------------------------------------

.PHONY: all luna-init luna-init-ctl luna-splash image run-qemu clean \
        test-unit test-fuzz lint verify help

all: luna-init luna-init-ctl luna-splash lgp-compositor lunagui luna-shell luna-desktop luna-installer luna-terminal luna-settings luna-files luna-calc luna-text luna-about luna-tasks build/live.lraw

luna-init: $(BUILD_DIR)/luna-init/luna-init

luna-init-ctl: $(BUILD_DIR)/luna-init-ctl/luna-init-ctl

luna-splash: $(BUILD_DIR)/luna-splash/luna-splash

build/synthwave.lraw: tools/gen_video.c
	@mkdir -p build
	@echo "  HOSTCC  tools/gen_video.c"
	@clang -O2 tools/gen_video.c -o build/gen_video -lm
	@echo "  GEN     build/synthwave.lraw"
	@build/gen_video build/synthwave.lraw

build/retro_lake.lraw: tools/convert_video.py build/synthwave.lraw
	@mkdir -p build
	@if [ -f "$(WALLPAPER_MP4)" ] && command -v ffmpeg >/dev/null 2>&1; then \
		echo "  CONVERT $(WALLPAPER_MP4) -> build/retro_lake.lraw"; \
		python3 tools/convert_video.py "$(WALLPAPER_MP4)" build/retro_lake.lraw 480 270 30 10; \
	else \
		if [ ! -f "$(WALLPAPER_MP4)" ]; then \
			echo "  WARN    $(WALLPAPER_MP4) not found; using generated fallback"; \
		else \
			echo "  WARN    ffmpeg not found; using generated fallback"; \
		fi; \
		cp build/synthwave.lraw build/retro_lake.lraw; \
	fi

build/live.lraw: build/synthwave.lraw build/retro_lake.lraw
	@echo "  COPY    build/live.lraw"
	@cp build/retro_lake.lraw build/live.lraw

# ---------------------------------------------------------------------------
# luna-init binary (statically linked — mandatory per 04_init_system.md)
# ---------------------------------------------------------------------------

$(BUILD_DIR)/luna-init/luna-init: $(LUNA_INIT_OBJECTS) | $(BUILD_DIR)/luna-init
	@echo "  LINK    $@"
	$(CC) $(CFLAGS_STATIC) -o $@ $^
	@echo "  SIZE    luna-init"
	@size $@

# luna-init objects — compile with static flags
$(BUILD_DIR)/luna-init/%.o: $(LUNA_INIT_SRC)/%.c | $(BUILD_DIR)/luna-init
	@echo "  CC      $<"
	$(CC) $(CFLAGS_STATIC) $(INCLUDES) -c -o $@ $<

# ---------------------------------------------------------------------------
# luna-init-ctl binary (dynamically linked — control CLI, not PID 1)
# ---------------------------------------------------------------------------

$(BUILD_DIR)/luna-init-ctl/luna-init-ctl: $(LUNA_CTL_OBJECTS) | $(BUILD_DIR)/luna-init-ctl
	@echo "  LINK    $@"
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/luna-init-ctl/%.o: $(LUNA_CTL_SRC)/%.c | $(BUILD_DIR)/luna-init-ctl
	@echo "  CC      $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ---------------------------------------------------------------------------
# luna-splash binary (statically linked for early boot)
# ---------------------------------------------------------------------------

$(BUILD_DIR)/luna-splash/luna-splash: $(LUNA_SPLASH_OBJECTS) | $(BUILD_DIR)/luna-splash
	@echo "  LINK    $@"
	$(CC) $(CFLAGS_STATIC) -o $@ $^
	@echo "  SIZE    luna-splash"
	@size $@

$(BUILD_DIR)/luna-splash/%.o: $(LUNA_SPLASH_SRC)/%.c | $(BUILD_DIR)/luna-splash
	@echo "  CC      $<"
	$(CC) $(CFLAGS_STATIC) $(INCLUDES) -c -o $@ $<

# ---------------------------------------------------------------------------
# Disk image (requires Linux / WSL2 — uses loopback devices)
# ---------------------------------------------------------------------------

image: all
	@echo "  IMAGE   Building Mahina disk image..."
	@bash $(SCRIPTS_DIR)/build-initramfs.sh
	@bash $(SCRIPTS_DIR)/build-image.sh
	@echo "  IMAGE   build/mahina-$(MAHINA_VERSION).img"

# ---------------------------------------------------------------------------
# QEMU launch
# Authority: Volume VII / 01_implementation_roadmap.md §0.1 Build Toolchain
# ---------------------------------------------------------------------------

QEMU_MEMORY  := 2G
QEMU_CPUS    := 2
QEMU_DISK    := build/mahina-$(MAHINA_VERSION).img
QEMU_FLAGS   := \
    -machine q35 \
    -cpu qemu64 \
    -m $(QEMU_MEMORY) \
    -smp $(QEMU_CPUS) \
    -drive file=$(QEMU_DISK),format=raw,if=virtio \
    -net nic,model=virtio \
    -net user \
    -bios /usr/share/ovmf/OVMF.fd \
    -vga virtio \
    -no-reboot

run-qemu: image
	@echo "  QEMU    Starting Mahina in QEMU..."
	@echo "  QEMU    Serial console: this terminal"
	@echo "  QEMU    Display: gtk window"
	qemu-system-x86_64 $(QEMU_FLAGS) -display gtk -serial stdio

# Run QEMU without display (headless — for CI/automated testing)
run-qemu-headless: image
	qemu-system-x86_64 $(QEMU_FLAGS) -display none -serial file:build/qemu-serial.log

# ---------------------------------------------------------------------------
# Unit tests (with mandatory sanitizers — Volume VI / 05_testing_standards.md)
# ---------------------------------------------------------------------------

test-unit: $(UNITY_OBJ) $(filter-out $(BUILD_DIR)/luna-init/main_test_asan.o, $(LUNA_INIT_OBJECTS:.o=_test_asan.o)) | $(BUILD_DIR)/tests
	@echo "  TEST    Running unit tests with ASan + UBSan"
	@failed=0; \
	for src in $(LUNA_INIT_TEST_SOURCES); do \
	    name=$$(basename $$src .c); \
	    echo "  BUILD   $$name"; \
	    $(CC) $(CFLAGS_TEST) $(INCLUDES) \
	        -I$(TESTS_DIR)/vendor/unity \
	        $$src \
	        $(filter-out $(BUILD_DIR)/luna-init/main_test_asan.o, $(LUNA_INIT_OBJECTS:.o=_test_asan.o)) \
	        $(UNITY_OBJ) \
	        -o $(BUILD_DIR)/tests/$$name 2>&1 || { failed=1; continue; }; \
	    echo "  RUN     $$name"; \
	    $(BUILD_DIR)/tests/$$name || failed=1; \
	done; \
	for src in $(LGP_UNIT_TEST_SOURCES); do \
	    name=lgp_$$(basename $$src .c); \
	    echo "  BUILD   $$name"; \
	    $(CC) $(CFLAGS_TEST) $(INCLUDES) \
	        -I$(TESTS_DIR)/vendor/unity \
	        $$src \
	        $(LGP_UNIT_TEST_SUPPORT) \
	        $(UNITY_OBJ) \
	        -o $(BUILD_DIR)/tests/$$name 2>&1 || { failed=1; continue; }; \
	    echo "  RUN     $$name"; \
	    $(BUILD_DIR)/tests/$$name || failed=1; \
	done; \
	for src in $(LUNA_GUI_UNIT_TEST_SOURCES); do \
	    name=gui_$$(basename $$src .c); \
	    echo "  BUILD   $$name"; \
	    $(CC) $(CFLAGS_TEST) $(INCLUDES) \
	        -I$(TESTS_DIR)/vendor/unity \
	        -I$(SRC_DIR)/luna-gui/include \
	        $$src \
	        $(LUNA_GUI_UNIT_TEST_SUPPORT) \
	        $(UNITY_OBJ) \
	        -o $(BUILD_DIR)/tests/$$name 2>&1 || { failed=1; continue; }; \
	    echo "  RUN     $$name"; \
	    $(BUILD_DIR)/tests/$$name || failed=1; \
	done; \
	if [ $$failed -eq 0 ]; then \
	    echo "  PASS    All unit tests passed"; \
	else \
	    echo "  FAIL    One or more unit tests failed"; \
	    exit 1; \
	fi

$(UNITY_OBJ): $(UNITY_SRC) | $(BUILD_DIR)/tests/vendor
	@echo "  CC      unity.c"
	$(CC) $(CFLAGS_TEST) -I$(TESTS_DIR)/vendor/unity -c -o $@ $<

# Recompile luna-init sources with test flags (ASan enabled)
$(BUILD_DIR)/luna-init/%_test_asan.o: $(LUNA_INIT_SRC)/%.c | $(BUILD_DIR)/luna-init
	$(CC) $(CFLAGS_TEST) $(INCLUDES) -c -o $@ $<

# ---------------------------------------------------------------------------
# Fuzz testing (AFL++ / libFuzzer)
# ---------------------------------------------------------------------------

test-fuzz: | $(BUILD_DIR)/tests
	@echo "  FUZZ    Building TOML fuzz target"
	$(CC) $(CFLAGS_FUZZ) $(INCLUDES) \
	    $(TESTS_DIR)/fuzz/toml/fuzz_toml.c \
	    $(LUNA_INIT_SRC)/toml.c \
	    -o $(BUILD_DIR)/tests/fuzz_toml
	@echo "  FUZZ    Building LGP TLV fuzz target"
	$(CC) $(CFLAGS_FUZZ) $(INCLUDES) \
	    $(TESTS_DIR)/fuzz/lgp_tlv/fuzz_lgp_tlv.c \
	    $(SRC_DIR)/lgp-compositor/protocol/tlv.c \
	    $(SRC_DIR)/lgp-compositor/protocol/surface.c \
	    -o $(BUILD_DIR)/tests/fuzz_lgp_tlv
	@rm -rf $(BUILD_DIR)/tests/fuzz_corpus
	@mkdir -p $(BUILD_DIR)/tests/fuzz_corpus/toml $(BUILD_DIR)/tests/fuzz_corpus/lgp_tlv
	@cp -R $(TESTS_DIR)/fuzz/toml/corpus/. $(BUILD_DIR)/tests/fuzz_corpus/toml/
	@cp -R $(TESTS_DIR)/fuzz/lgp_tlv/corpus/. $(BUILD_DIR)/tests/fuzz_corpus/lgp_tlv/
	@echo "  FUZZ    Running fuzz regression (5 seconds)"
	$(BUILD_DIR)/tests/fuzz_toml \
	    $(BUILD_DIR)/tests/fuzz_corpus/toml/ \
	    -max_total_time=5 \
	    -print_final_stats=1
	$(BUILD_DIR)/tests/fuzz_lgp_tlv \
	    $(BUILD_DIR)/tests/fuzz_corpus/lgp_tlv/ \
	    -max_total_time=5 \
	    -print_final_stats=1

# ---------------------------------------------------------------------------
# Integrated Verification Loop (Stage 2 Integration Boot Smoke Test)
# ---------------------------------------------------------------------------

verify: clean all test-unit image
	@echo "  VERIFY  Running QEMU headless smoke test..."
	@rm -f build/qemu-serial.log
	@qemu-system-x86_64 $(QEMU_FLAGS) -display none -serial file:build/qemu-serial.log & \
	pid=$$!; \
	sleep 5; \
	kill $$pid 2>/dev/null || true; \
	wait $$pid 2>/dev/null || true
	@if grep -q "FATAL" build/qemu-serial.log; then \
	    echo "  FAIL    Smoke test failed: FATAL found in serial log"; \
	    cat build/qemu-serial.log; \
	    exit 1; \
	elif grep -q "interactive shell" build/qemu-serial.log || grep -q "luna-init" build/qemu-serial.log; then \
	    echo "  PASS    Smoke test passed! Mahina booted successfully."; \
	else \
	    echo "  FAIL    Smoke test failed: Mahina did not boot correctly (no expected output in serial log)"; \
	    cat build/qemu-serial.log; \
	    exit 1; \
	fi

# ---------------------------------------------------------------------------
# Static analysis (clang-tidy)
# Authority: Volume VI / 01_coding_standards.md §5, Volume VI / 05_testing_standards.md §CI Gate 1
# ---------------------------------------------------------------------------

lint:
	@echo "  LINT    Running clang-tidy on all sources"
	clang-tidy \
	    --checks='-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,cert-*,-cert-err33-c,-cert-err34-c,bugprone-*,-bugprone-easily-swappable-parameters,performance-*,portability-*,readability-*,-readability-magic-numbers,-readability-identifier-length,-readability-braces-around-statements,-readability-math-missing-parentheses,-readability-function-cognitive-complexity' \
	    --warnings-as-errors='*' \
	    $(LUNA_INIT_SOURCES) $(LUNA_CTL_SOURCES) $(LUNA_SPLASH_SOURCES) $(LGP_COMPOSITOR_SOURCES) \
	    -- $(CFLAGS) $(INCLUDES) -I/usr/include/libdrm

# ---------------------------------------------------------------------------
# Directory creation
# ---------------------------------------------------------------------------
$(BUILD_DIR)/luna-init:
	@mkdir -p $@

$(BUILD_DIR)/luna-init-ctl:
	@mkdir -p $@

$(BUILD_DIR)/luna-splash:
	@mkdir -p $@

$(BUILD_DIR)/tests:
	@mkdir -p $@

$(BUILD_DIR)/tests/vendor:
	@mkdir -p $@

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

clean:
	@echo "  CLEAN   Removing build artifacts"
	rm -rf $(BUILD_DIR)
	@echo "  CLEAN   Done"

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------

help:
	@echo "Mahina Build System — Version $(MAHINA_VERSION) ($(MAHINA_CODENAME))"
	@echo ""
	@echo "Targets:"
	@echo "  make all              Build luna-init + luna-init-ctl"
	@echo "  make luna-init        Build luna-init only (statically linked)"
	@echo "  make luna-init-ctl    Build luna-init-ctl only"
	@echo "  make image            Build bootable disk image (requires WSL2/Linux)"
	@echo "  make run-qemu         Build image and launch in QEMU"
	@echo "  make test-unit        Run unit tests with ASan + UBSan"
	@echo "  make test-fuzz        Run fuzz regression on TOML parser"
	@echo "  make lint             Run clang-tidy static analysis"
	@echo "  make clean            Remove build artifacts"
	@echo ""
	@echo "Toolchain: clang (CC=$(CC))"
	@echo "Build env: WSL2 or Linux required for image/run-qemu targets"

# ---------------------------------------------------------------------------
# lgp-compositor
# ---------------------------------------------------------------------------
include src/lgp-compositor/Makefile.inc

# ---------------------------------------------------------------------------
# LunaGUI
# ---------------------------------------------------------------------------
include src/luna-gui/Makefile.inc

# ---------------------------------------------------------------------------
# luna-shell
# ---------------------------------------------------------------------------
include src/luna-shell/Makefile.inc

# ---------------------------------------------------------------------------
# luna-desktop
# ---------------------------------------------------------------------------
include src/luna-desktop/Makefile.inc

# ---------------------------------------------------------------------------
# luna-installer
# ---------------------------------------------------------------------------
include src/luna-installer/Makefile.inc

# ---------------------------------------------------------------------------
# luna-terminal
# ---------------------------------------------------------------------------
include src/luna-terminal/Makefile.inc

# ---------------------------------------------------------------------------
# luna-settings
# ---------------------------------------------------------------------------
include src/luna-settings/Makefile.inc

# ---------------------------------------------------------------------------
# luna-files
# ---------------------------------------------------------------------------
include src/luna-files/Makefile.inc

# ---------------------------------------------------------------------------
# luna-calc
# ---------------------------------------------------------------------------
include src/luna-calc/Makefile.inc

# ---------------------------------------------------------------------------
# luna-text
# ---------------------------------------------------------------------------
include src/luna-text/Makefile.inc

# ---------------------------------------------------------------------------
# luna-about
# ---------------------------------------------------------------------------
include src/luna-about/Makefile.inc

# ---------------------------------------------------------------------------
# luna-tasks
# ---------------------------------------------------------------------------
include src/luna-tasks/Makefile.inc
