# ---------------------------------------------------------------------------
# LunaGUI Toolkit (Phase 3)
# ---------------------------------------------------------------------------

LUNAGUI_SRC := $(SRC_DIR)/luna-gui
BUILD_LUNAGUI := $(BUILD_DIR)/luna-gui

LUNAGUI_SOURCES := \
    $(LUNAGUI_SRC)/core/application.c \
    $(LUNAGUI_SRC)/core/window.c \
    $(LUNAGUI_SRC)/core/keyboard.c \
    $(LUNAGUI_SRC)/render/canvas.c \
    $(LUNAGUI_SRC)/render/font.c \
    $(LUNAGUI_SRC)/render/render.c \
    $(LUNAGUI_SRC)/widgets/widget.c \
    $(LUNAGUI_SRC)/widgets/button.c \
    $(LUNAGUI_SRC)/widgets/label.c \
    $(LUNAGUI_SRC)/widgets/vbox.c \
    $(LUNAGUI_SRC)/widgets/hbox.c \
    $(LUNAGUI_SRC)/widgets/canvas_widget.c \
    $(LUNAGUI_SRC)/widgets/scroll.c

LUNAGUI_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LUNAGUI_SOURCES))
LUNAGUI_LIB := $(BUILD_LUNAGUI)/liblunagui.a

LUNAGUI_CFLAGS := $(CFLAGS_STATIC) -I$(LUNAGUI_SRC)/include -I$(SRC_DIR)

all: $(LUNAGUI_LIB)

.PHONY: lunagui
lunagui: $(LUNAGUI_LIB)

$(LUNAGUI_LIB): $(LUNAGUI_OBJECTS) | $(BUILD_LUNAGUI)
	@echo "  AR      $@"
	$(AR) rcs $@ $^

$(BUILD_DIR)/luna-gui/%.o: $(SRC_DIR)/luna-gui/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(LUNAGUI_CFLAGS) -c -o $@ $<
