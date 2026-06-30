# ---------------------------------------------------------------------------
# luna-terminal (Terminal Mission)
# ---------------------------------------------------------------------------

LUNA_TERMINAL_SRC := $(SRC_DIR)/luna-terminal
BUILD_LUNA_TERMINAL := $(BUILD_DIR)/luna-terminal

LUNA_TERMINAL_SOURCES := \
    $(LUNA_TERMINAL_SRC)/main.c \
    $(LUNA_TERMINAL_SRC)/pty.c \
    $(LUNA_TERMINAL_SRC)/ansi.c

LUNA_TERMINAL_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNA_TERMINAL_SOURCES))

all: luna-terminal

.PHONY: luna-terminal
luna-terminal: $(BUILD_LUNA_TERMINAL)/luna-terminal

LUNA_TERMINAL_CFLAGS := $(CFLAGS_STATIC) -I$(SRC_DIR)/luna-gui/include -I$(SRC_DIR)

$(BUILD_LUNA_TERMINAL)/luna-terminal: $(LUNA_TERMINAL_OBJECTS) $(LUNAGUI_LIB) | $(BUILD_LUNA_TERMINAL)
	@echo "  LINK    $@"
	$(CC) $(LUNA_TERMINAL_CFLAGS) -lutil -o $@ $^
	@echo "  SIZE    luna-terminal"
	@size $@

$(BUILD_DIR)/luna-terminal/%.o: $(SRC_DIR)/luna-terminal/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(LUNA_TERMINAL_CFLAGS) -c -o $@ $<
