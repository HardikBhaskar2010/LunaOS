# ---------------------------------------------------------------------------
# luna-shell (Desktop Shell & WM — Luna Island)
# ---------------------------------------------------------------------------

LUNA_SHELL_SRC := $(SRC_DIR)/luna-shell
BUILD_LUNA_SHELL := $(BUILD_DIR)/luna-shell

LUNA_SHELL_SOURCES := \
    $(LUNA_SHELL_SRC)/main.c \
    $(LUNA_SHELL_SRC)/dock.c \
    $(LUNA_SHELL_SRC)/decorations.c \
    $(LUNA_SHELL_SRC)/topbar.c

LUNA_SHELL_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_SHELL_SOURCES))
LUNA_WALLPAPER_RS_SRC := $(SRC_DIR)/luna-wallpaper-rs
LUNA_WALLPAPER_RS_LIB := $(BUILD_DIR)/luna-wallpaper-rs/libluna_wallpaper.a

all: luna-shell

.PHONY: luna-shell
luna-shell: $(BUILD_LUNA_SHELL)/luna-shell

LUNA_SHELL_CFLAGS := $(CFLAGS_STATIC) -I$(SRC_DIR)/luna-gui/include -I$(SRC_DIR)
LUNA_SHELL_RUST_LIBS := -lpthread -ldl -lm -lrt -lutil

$(LUNA_WALLPAPER_RS_LIB): $(LUNA_WALLPAPER_RS_SRC)/Cargo.toml $(LUNA_WALLPAPER_RS_SRC)/src/lib.rs | $(BUILD_LUNA_SHELL)
	@echo "  CARGO   luna-wallpaper-rs"
	@cd $(LUNA_WALLPAPER_RS_SRC) && cargo build --release
	@mkdir -p $(dir $@)
	@cp $(LUNA_WALLPAPER_RS_SRC)/target/release/libluna_wallpaper.a $@

$(BUILD_LUNA_SHELL)/luna-shell: $(LUNA_SHELL_OBJECTS) $(LUNAGUI_LIB) $(LUNA_WALLPAPER_RS_LIB) | $(BUILD_LUNA_SHELL)
	@echo "  LINK    $@"
	$(CC) $(LUNA_SHELL_CFLAGS) -o $@ $^ $(LUNA_SHELL_RUST_LIBS)
	@echo "  SIZE    luna-shell"
	@size $@

$(BUILD_DIR)/luna-shell/%.o: $(SRC_DIR)/luna-shell/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(LUNA_SHELL_CFLAGS) -c -o $@ $<
