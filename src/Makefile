###########################################################
# Supported platforms
#   tg5040 / tg5050 : ARM64 device builds (cross-compiled in the Docker toolchain)
#   desktop         : native Linux/macOS build for local dev/testing (no device,
#                     no cross-compiler)
SUPPORTED_PLATFORMS = tg5040 tg5050 desktop
DEFAULT_PLATFORM = tg5040

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif
ifeq (,$(PLATFORM))
PLATFORM=$(DEFAULT_PLATFORM)
endif
ifeq (,$(filter $(PLATFORM),$(SUPPORTED_PLATFORMS)))
$(error Invalid PLATFORM '$(PLATFORM)'. Supported: $(SUPPORTED_PLATFORMS))
endif

# The desktop target compiles natively, so CROSS_COMPILE is expected to be
# empty (CC falls back to the host gcc). Device targets must have a cross
# toolchain.
ifneq ($(PLATFORM), desktop)
ifeq (,$(CROSS_COMPILE))
$(error Missing CROSS_COMPILE for this toolchain)
endif
endif

###########################################################
# Where the shared NextUI sources live.
#
# Inside the Docker cross-toolchain (see ../run-docker.sh) this repo and the
# NextUI workspace's all/ and $(PLATFORM)/ dirs are bind-mounted as siblings,
# so the plain relative default resolves correctly -- same layout
# workspace/all/parental's own makefile assumes. Local desktop builds don't
# have that physical adjacency (this repo isn't nested inside the NextUI
# checkout), so ../build-desktop.sh overrides these two on the command line,
# resolved through the .nextui-workspace symlink.
NEXTUI_ALL      ?= ../../all
NEXTUI_PLATFORM ?= ../../$(PLATFORM)/platform

# Pulls in the per-platform CFLAGS/LDFLAGS (SDL2/GLES linking, -DUSE_SDL2,
# PREFIX/PREFIX_LOCAL include+lib paths) that every NextUI pak builds
# against -- see e.g. workspace/all/parental/makefile for the precedent.
# desktop/platform/makefile.env branches on UNAME_S (set by NextUI's own
# makefile.native when it's the one driving the build); our Makefile is
# invoked standalone by build-desktop.sh, so without this the Linux/Darwin
# branches (pkg-config cflags, -DUSE_GL -DGL_GLEXT_PROTOTYPES) are silently
# skipped and GL calls in platform.c fail to compile with implicit-declaration
# errors. Device platforms' makefile.env don't key off UNAME_S, so this is a
# no-op for tg5040/tg5050.
UNAME_S := $(shell uname -s)
include $(NEXTUI_PLATFORM)/makefile.env
SDL?=SDL

###########################################################
# libsmb2 (vendored in ../lib/libsmb2, built separately per platform by
# ../lib/build-libsmb2.sh -- see ../build-desktop.sh / ../build-tg5040.sh /
# ../build-tg5050.sh, which run it before invoking this Makefile).
SMB2_DIR   = ../lib/libsmb2
SMB2_BUILD = ../lib/build/libsmb2-$(PLATFORM)

###########################################################

TARGET  = sambasync
PRODUCT = ../bin/$(PLATFORM)/$(TARGET).elf

BUILD_DIR = build/$(PLATFORM)
OBJ_DIR   = $(BUILD_DIR)/obj

CC = $(CROSS_COMPILE)gcc

INCDIR = -I. -I$(NEXTUI_ALL)/common -I$(NEXTUI_PLATFORM) -I$(SMB2_DIR)/include

###########################################################
# Sources
#
# Shared NextUI common/ code and per-platform code, each built into their own
# object subdirectory (not $(OBJ_DIR)/%.o) so objects for one platform never
# collide with another's, and so the two source dirs never collide with each
# other despite both being reached through a variable prefix.

LOCAL_SRC     = main.c ui.c sync_config.c link_state.c smb_client.c browse.c local_fs.c sync_engine.c sync_queue.c screens/links_list.c screens/link_detail.c
VENDOR_SRC    = vendor/tomlc17/tomlc17.c
COMMON_SRC    = utils.c api.c config.c scaler.c
PLATFORM_SRC  = platform.c

LOCAL_OBJS    = $(patsubst %.c,$(OBJ_DIR)/%.o,$(LOCAL_SRC))
VENDOR_OBJS   = $(patsubst %.c,$(OBJ_DIR)/%.o,$(VENDOR_SRC))
COMMON_OBJS   = $(patsubst %.c,$(OBJ_DIR)/ext/common/%.o,$(COMMON_SRC))
PLATFORM_OBJS = $(patsubst %.c,$(OBJ_DIR)/ext/platform/%.o,$(PLATFORM_SRC))
APP_OBJS      = $(LOCAL_OBJS) $(VENDOR_OBJS) $(COMMON_OBJS) $(PLATFORM_OBJS)

###########################################################
# Compiler / linker flags

MY_CFLAGS  = $(OPT)
MY_CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
MY_CFLAGS += -MMD -MP

MY_LDFLAGS = -lmsettings -L$(SMB2_BUILD)/lib -lsmb2

###########################################################

.PHONY: all clean

all: $(PRODUCT)

$(PRODUCT): $(APP_OBJS)
	@mkdir -p $(@D)
	$(CC) $(APP_OBJS) $(CFLAGS) $(MY_CFLAGS) -o $@ $(LDFLAGS) $(MY_LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(MY_CFLAGS) -o $@ $<

# tomlc17 needs C11 (static_assert); the later -std wins over MY_CFLAGS' gnu99.
$(VENDOR_OBJS): $(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(MY_CFLAGS) -std=gnu11 -o $@ $<

$(OBJ_DIR)/ext/common/%.o: $(NEXTUI_ALL)/common/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(MY_CFLAGS) -o $@ $<

$(OBJ_DIR)/ext/platform/%.o: $(NEXTUI_PLATFORM)/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(MY_CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(PRODUCT)

-include $(APP_OBJS:.o=.d)
