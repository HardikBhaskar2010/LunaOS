# ---------------------------------------------------------------------------
# lgp-compositor sources (M0 - M3)
# ---------------------------------------------------------------------------

LGP_COMPOSITOR_SRC := $(SRC_DIR)/lgp-compositor
BUILD_LGP          := $(BUILD_DIR)/lgp-compositor

LGP_COMPOSITOR_SOURCES := \
    $(LGP_COMPOSITOR_SRC)/main.c \
    $(LGP_COMPOSITOR_SRC)/config/args.c \
    $(LGP_COMPOSITOR_SRC)/logging/log.c \
    $(LGP_COMPOSITOR_SRC)/util/signal.c \
    $(LGP_COMPOSITOR_SRC)/drm/device.c \
    $(LGP_COMPOSITOR_SRC)/drm/connector.c \
    $(LGP_COMPOSITOR_SRC)/kms/crtc.c \
    $(LGP_COMPOSITOR_SRC)/kms/dumb_buffer.c \
    $(LGP_COMPOSITOR_SRC)/kms/page_flip.c \
    $(LGP_COMPOSITOR_SRC)/ipc/socket_server.c \
    $(LGP_COMPOSITOR_SRC)/ipc/client.c \
    $(LGP_COMPOSITOR_SRC)/protocol/tlv.c \
    $(LGP_COMPOSITOR_SRC)/protocol/hello.c \
    $(LGP_COMPOSITOR_SRC)/protocol/caps.c \
    $(LGP_COMPOSITOR_SRC)/protocol/surface.c \
    $(LGP_COMPOSITOR_SRC)/scene/surface.c

LGP_COMPOSITOR_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LGP_COMPOSITOR_SOURCES))

# test client
LGP_TEST_CLIENT_SRC := $(LGP_COMPOSITOR_SRC)/test_client.c
LGP_TEST_CLIENT_OBJ := $(BUILD_LGP)/test_client.o

all: lgp-compositor lgp-test-client

.PHONY: lgp-compositor lgp-test-client
lgp-compositor: $(BUILD_LGP)/lgp-compositor
lgp-test-client: $(BUILD_LGP)/lgp-test-client

# lgp-compositor is statically linked to avoid needing glibc and libdrm in the image
LGP_CFLAGS := $(CFLAGS) -I/usr/include/libdrm -static
LGP_LDFLAGS := -ldrm

$(BUILD_LGP)/lgp-compositor: $(LGP_COMPOSITOR_OBJECTS) | $(BUILD_LGP)
	@echo "  LINK    $@"
	$(CC) $(LGP_CFLAGS) -o $@ $^ $(LGP_LDFLAGS)
	@echo "  SIZE    lgp-compositor"
	@size $@

# Ensure subdirectories exist in build directory
$(BUILD_LGP)/%.o: $(LGP_COMPOSITOR_SRC)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(LGP_CFLAGS) $(INCLUDES) -c -o $@ $<

# Test client binary
$(BUILD_LGP)/lgp-test-client: $(LGP_TEST_CLIENT_OBJ) | $(BUILD_LGP)
	@echo "  LINK    $@"
	$(CC) $(CFLAGS) -static -o $@ $^
	@echo "  SIZE    lgp-test-client"
	@size $@
