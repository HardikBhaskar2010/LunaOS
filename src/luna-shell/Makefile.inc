# ---------------------------------------------------------------------------
# luna-shell (Desktop Shell & WM)
# ---------------------------------------------------------------------------

LUNA_SHELL_SRC := $(SRC_DIR)/luna-shell
BUILD_LUNA_SHELL := $(BUILD_DIR)/luna-shell

LUNA_SHELL_SOURCES := \
    $(LUNA_SHELL_SRC)/main.c

LUNA_SHELL_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_SHELL_SOURCES))

all: luna-shell

.PHONY: luna-shell
luna-shell: $(BUILD_LUNA_SHELL)/luna-shell

LUNA_SHELL_CFLAGS := $(CFLAGS_STATIC) -I$(SRC_DIR)/luna-gui/include -I$(SRC_DIR)

$(BUILD_LUNA_SHELL)/luna-shell: $(LUNA_SHELL_OBJECTS) $(LUNAGUI_LIB) | $(BUILD_LUNA_SHELL)
	@echo "  LINK    $@"
	$(CC) $(LUNA_SHELL_CFLAGS) -o $@ $^
	@echo "  SIZE    luna-shell"
	@size $@

$(BUILD_DIR)/luna-shell/%.o: $(SRC_DIR)/luna-shell/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(LUNA_SHELL_CFLAGS) -c -o $@ $<
