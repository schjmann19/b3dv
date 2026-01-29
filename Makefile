# B3DV Makefile with local raylib support
# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lraylib -lm

# Local raylib support
LOCAL_RAYLIB_DIR = external/raylib
LOCAL_RAYLIB_LIB = $(LOCAL_RAYLIB_DIR)/src/libraylib.a

# Check if using local raylib (set by build script)
ifdef RAYLIB_CFLAGS
    CFLAGS += $(RAYLIB_CFLAGS)
    LDFLAGS = -lm
endif

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    PLATFORM_LIBS = -lGL -lpthread -ldl -lrt -lX11
    PLATFORM = PLATFORM_DESKTOP
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM_LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    PLATFORM = PLATFORM_DESKTOP
endif
ifeq ($(UNAME_S),FreeBSD)
    PLATFORM_LIBS = -lGL -lpthread -lrt -lX11
    PLATFORM = PLATFORM_DESKTOP
endif
ifeq ($(UNAME_S),OpenBSD)
    PLATFORM_LIBS = -lGL -lpthread -lrt -lX11
    PLATFORM = PLATFORM_DESKTOP
endif

# Windows cross-compilation
WIN_CC = x86_64-w64-mingw32-gcc
WIN_RAYLIB_PATH = ./external/raylib
WIN_CFLAGS = -Wall -Wextra -O2 -march=x86-64 -mtune=generic -I$(WIN_RAYLIB_PATH)/src
WIN_LDFLAGS = -L$(WIN_RAYLIB_PATH)/src -l:libraylib.a -lopengl32 -lgdi32 -lwinmm -lpsapi -lm -static-libgcc

SOURCES = src/main.c src/world_generation.c src/vec_math.c src/rendering.c src/utils.c
HEADERS = src/world.h src/vec_math.h src/rendering.h src/utils.h

# Default target
all: b3dv

# Main build target
b3dv: $(SOURCES) $(HEADERS)
	$(CC) $(SOURCES) -o b3dv $(CFLAGS) $(LDFLAGS) $(PLATFORM_LIBS)

# Build with local raylib
local: check-local-raylib
	$(CC) $(SOURCES) -o b3dv $(CFLAGS) \
		-I$(LOCAL_RAYLIB_DIR)/src \
		-L$(LOCAL_RAYLIB_DIR)/src \
		-l:libraylib.a \
		-lm $(PLATFORM_LIBS)

# Check and build local raylib if needed
check-local-raylib:
	@if [ ! -f $(LOCAL_RAYLIB_LIB) ]; then \
		echo "Building local raylib..."; \
		mkdir -p external; \
		if [ ! -d $(LOCAL_RAYLIB_DIR) ]; then \
			if command -v git >/dev/null 2>&1; then \
				git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git $(LOCAL_RAYLIB_DIR); \
			else \
				echo "Error: git required to download raylib"; \
				exit 1; \
			fi; \
		fi; \
		cd $(LOCAL_RAYLIB_DIR)/src && $(MAKE) PLATFORM=$(PLATFORM) RAYLIB_LIBTYPE=STATIC; \
	fi

# Check and build local raylib for Windows
check-local-raylib-windows:
	@echo "Building local raylib for Windows..."; \
	mkdir -p external; \
	if [ ! -d $(LOCAL_RAYLIB_DIR) ]; then \
		if command -v git >/dev/null 2>&1; then \
			git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git $(LOCAL_RAYLIB_DIR); \
		else \
			echo "Error: git required to download raylib"; \
			exit 1; \
		fi; \
	fi; \
	cd $(LOCAL_RAYLIB_DIR)/src && $(MAKE) CC=$(WIN_CC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC clean && $(MAKE) CC=$(WIN_CC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC

# Windows build
.PHONY: windows
windows: check-local-raylib-windows $(SOURCES) $(HEADERS)
	@echo "Building for Windows (MinGW-w64)..."
	$(WIN_CC) $(SOURCES) -o b3dv.exe $(WIN_CFLAGS) $(WIN_LDFLAGS)
	@echo "Windows build complete: b3dv.exe"

# Platform-specific targets
.PHONY: linux
linux: all

.PHONY: macos
macos: all

.PHONY: freebsd
freebsd: all

.PHONY: openbsd
openbsd: all

# Clean
clean:
	rm -f b3dv b3dv.exe *.o

# Deep clean (including local raylib)
distclean: clean
	rm -rf external/

# Help
help:
	@echo "B3DV Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build with system raylib"
	@echo "  local         - Build with local raylib (auto-downloads if needed)"
	@echo "  windows       - Cross-compile for Windows"
	@echo "  clean         - Remove build artifacts"
	@echo "  distclean     - Remove build artifacts and local raylib"
	@echo ""
	@echo "Platform targets: linux, macos, freebsd, openbsd"

.PHONY: all clean distclean help check-local-raylib check-local-raylib-windows