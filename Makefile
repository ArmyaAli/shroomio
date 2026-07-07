#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
#shroomio Makefile
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
#Build system for the shroomio multiplayer arena game.
#
#Sections:
# 1. Project Configuration - Core project settings and version info
# 2. Directory Paths - Source, build, and output directories
# 3. Compiler Settings - Compilers, flags, and libraries
# 4. Source Files - Lists of source files and objects
# 5. Build Targets - Main build targets(linux, windows, server)
# 6. Compilation Rules - Pattern rules for compiling objects
# 7. Vendor Dependencies - raylib, ImGui, enet, Unity downloads
# 8. Test Targets - Unit test compilation and execution
# 9. Docker Targets - Container builds for server and devcontainer
# 10. Lint & Format - Code formatting and static analysis
# 11. Documentation - LaTeX specification build
# 12. Cleanup - Clean and distclean targets
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =

#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
# 1. Project Configuration
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
PROJECT := shroomio

IMGUI_VERSION  := 1.91.8
UNITY_VERSION  := 2.6.0

#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
# 2. Directory Paths
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
SRC_DIR         := src
CLIENT_SRC_DIR  := $(SRC_DIR)/client
SERVER_SRC_DIR  := $(SRC_DIR)/server
SHARED_SRC_DIR  := $(SRC_DIR)/shared
BUILD_DIR       := build
DIST_DIR        := dist
TESTS_DIR       := tests
UNIT_TESTS_DIR  := $(TESTS_DIR)/unit
IMGUI_TESTS_DIR := $(TESTS_DIR)/imgui

LINUX_BUILD_DIR   := $(BUILD_DIR)/linux
WINDOWS_BUILD_DIR := $(BUILD_DIR)/windows
MACOS_BUILD_DIR   := $(BUILD_DIR)/macos
TEST_BUILD_DIR    := $(BUILD_DIR)/tests
COVERAGE_DIR      := coverage

#Output binaries
CLIENT_LINUX_BIN := $(DIST_DIR)/linux/client/$(PROJECT)
CLIENT_WINDOWS_BIN := $(DIST_DIR)/windows/client/$(PROJECT).exe
CLIENT_MACOS_BIN := $(DIST_DIR)/macos/client/$(PROJECT)
SERVER_LINUX_BIN := $(DIST_DIR)/linux/server/$(PROJECT)-server
SERVER_WINDOWS_BIN := $(DIST_DIR)/windows/server/$(PROJECT)-server.exe
SERVER_MACOS_BIN := $(DIST_DIR)/macos/server/$(PROJECT)-server

#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
# 3. Dependency Configuration
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
VCPKG_ROOT := $(CURDIR)/vcpkg
VCPKG_BIN := $(VCPKG_ROOT)/vcpkg
VCPKG_INSTALLED_DIR := $(CURDIR)/vcpkg_installed
VCPKG_LINUX_TRIPLET := x64-linux
VCPKG_WINDOWS_TRIPLET := x64-mingw-static
VCPKG_MACOS_TRIPLET := x64-osx
VCPKG_LINUX_INSTALLED_DIR := $(VCPKG_INSTALLED_DIR)/linux
VCPKG_WINDOWS_INSTALLED_DIR := $(VCPKG_INSTALLED_DIR)/windows
VCPKG_MACOS_INSTALLED_DIR := $(VCPKG_INSTALLED_DIR)/macos
VCPKG_LINUX_PREFIX := $(VCPKG_LINUX_INSTALLED_DIR)/$(VCPKG_LINUX_TRIPLET)
VCPKG_WINDOWS_PREFIX := $(VCPKG_WINDOWS_INSTALLED_DIR)/$(VCPKG_WINDOWS_TRIPLET)
VCPKG_MACOS_PREFIX := $(VCPKG_MACOS_INSTALLED_DIR)/$(VCPKG_MACOS_TRIPLET)
VCPKG_LINUX_INCLUDE_DIR := $(VCPKG_LINUX_PREFIX)/include
VCPKG_WINDOWS_INCLUDE_DIR := $(VCPKG_WINDOWS_PREFIX)/include
VCPKG_MACOS_INCLUDE_DIR := $(VCPKG_MACOS_PREFIX)/include
VCPKG_LINUX_LIB_DIR := $(VCPKG_LINUX_PREFIX)/lib
VCPKG_WINDOWS_LIB_DIR := $(VCPKG_WINDOWS_PREFIX)/lib
VCPKG_MACOS_LIB_DIR := $(VCPKG_MACOS_PREFIX)/lib
VCPKG_LINUX_STAMP := $(VCPKG_LINUX_PREFIX)/.vcpkg-ready
VCPKG_WINDOWS_STAMP := $(VCPKG_WINDOWS_PREFIX)/.vcpkg-ready
VCPKG_MACOS_STAMP := $(VCPKG_MACOS_PREFIX)/.vcpkg-ready

# Test-only ImGui source mirror kept for the manual ImGui Test Engine harness.
IMGUI_DIR := vendor/imgui-$(IMGUI_VERSION)
IMGUI_URL := https://github.com/ocornut/imgui/archive/refs/tags/v$(IMGUI_VERSION).tar.gz
IMGUI_SRC_DIR := $(IMGUI_DIR)

IMGUI_TEST_ENGINE_REF := d3d44963413cfc64c80a67aa0acf953021dd7636
IMGUI_TEST_ENGINE_DIR := vendor/imgui_test_engine-$(IMGUI_TEST_ENGINE_REF)
IMGUI_TEST_ENGINE_URL := https://github.com/ocornut/imgui_test_engine/archive/$(IMGUI_TEST_ENGINE_REF).tar.gz
IMGUI_TEST_ENGINE_SRC_DIR := $(IMGUI_TEST_ENGINE_DIR)/imgui_test_engine

UNITY_DIR := vendor/Unity-$(UNITY_VERSION)
UNITY_URL := https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v$(UNITY_VERSION).tar.gz
UNITY_SRC_DIR := $(UNITY_DIR)/src
UNITY_SRC := $(UNITY_SRC_DIR)/unity.c
UNITY_INCLUDE := -I$(UNITY_SRC_DIR)

#Warning flags
COMMON_WARNINGS := -Wall -Wextra -Wpedantic
VENDOR_WARNINGS := -Wall -Wextra

#Include directories
COMMON_INCLUDE_DIRS := -I$(SRC_DIR) -I$(CLIENT_SRC_DIR) -I$(SERVER_SRC_DIR) -I$(SHARED_SRC_DIR)

#Test compiler flags
TEST_CFLAGS := -std=c11 -O0 -g $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
               $(UNITY_INCLUDE) -DTEST_MODE -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
TEST_LIBS   := -lm
COVERAGE_CFLAGS := $(TEST_CFLAGS) -fprofile-arcs -ftest-coverage
COVERAGE_LIBS := $(TEST_LIBS) -lgcov
IMGUI_TEST_CFLAGS := -std=c11 -O0 -g $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
	-I$(VCPKG_LINUX_INCLUDE_DIR) \
	-DTEST_MODE -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
IMGUI_TEST_CXXFLAGS := -O0 -g $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
	-I. \
	-I$(IMGUI_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) -I$(IMGUI_TEST_ENGINE_DIR) -I$(IMGUI_TEST_ENGINE_SRC_DIR) \
	-DTEST_MODE -DIMGUI_USER_CONFIG=\"tests/imgui/shroom_imgui_test_imconfig.h\" -pthread
IMGUI_TEST_ENGINE_CXXFLAGS := $(IMGUI_TEST_CXXFLAGS) \
	-DImGuiTabBarFlags_FittingPolicyMixed=ImGuiTabBarFlags_FittingPolicyDefault_

#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
# 4. Compiler Settings
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
LINUX_CC    ?= cc
WINDOWS_CC  ?= x86_64-w64-mingw32-gcc
MACOS_CC    ?= cc
LINUX_CXX   ?= c++
WINDOWS_CXX ?= x86_64-w64-mingw32-g++
MACOS_CXX   ?= c++
AR          ?= ar

#Tools
CURL    ?= curl
TAR     ?= tar
MKDIR_P ?= mkdir -p
RM_RF   ?= rm -rf
DOCKER  ?= docker

#Client compiler flags(raylib/imgui/enet provided by vcpkg)
COMMON_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS)
COMMON_CXXFLAGS := -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) -I.
LINUX_CFLAGS   := $(COMMON_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
WINDOWS_CFLAGS := $(COMMON_CFLAGS) -I$(VCPKG_WINDOWS_INCLUDE_DIR) -DPLATFORM_DESKTOP
WINDOWS_CFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER
MACOS_CFLAGS   := $(COMMON_CFLAGS) -I$(VCPKG_MACOS_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
LINUX_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
WINDOWS_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_WINDOWS_INCLUDE_DIR) -DPLATFORM_DESKTOP
WINDOWS_CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER
MACOS_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_MACOS_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE

#Server compiler flags(headless, ENet - based)
LINUX_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_LINUX_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
WINDOWS_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_WINDOWS_INCLUDE_DIR)
WINDOWS_SERVER_CFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER
MACOS_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_MACOS_INCLUDE_DIR) -D_DEFAULT_SOURCE
LINUX_SERVER_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -lenet -lm -lsqlite3
WINDOWS_SERVER_LIBS := -L$(VCPKG_WINDOWS_LIB_DIR) -lenet -lsqlite3 -lws2_32 -lwinmm
MACOS_SERVER_LIBS := -L$(VCPKG_MACOS_LIB_DIR) -lenet -lsqlite3 -lm

#Test compiler flags(UNITY_INCLUDE defined in vendor section)
TEST_LIBS := -lm

#Platform link libraries
LINUX_LIBS   := -lGL -lm -ldl -lpthread -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lasound
WINDOWS_LIBS := -lopengl32 -lgdi32 -lwinmm -lws2_32
MACOS_LIBS   := -lm -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
LINUX_THIRD_PARTY_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -limgui -lenet -lraylib -lglfw3
WINDOWS_THIRD_PARTY_LIBS := -L$(VCPKG_WINDOWS_LIB_DIR) -limgui -lenet -lraylib -lglfw3
MACOS_THIRD_PARTY_LIBS := -L$(VCPKG_MACOS_LIB_DIR) -limgui -lenet -lraylib -lglfw3
IMGUI_TEST_THIRD_PARTY_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -lenet -lraylib -lglfw3

#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =
# 5. Source Files and Objects
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==   \
    == == == == == == =

#Dear ImGui wrapper source files (core library provided by vcpkg)
LINUX_IMGUI_OBJECTS := $(LINUX_BUILD_DIR)/client/imgui_impl_raylib.o \
	$(LINUX_BUILD_DIR)/client/imgui_wrapper.o
WINDOWS_IMGUI_OBJECTS := $(WINDOWS_BUILD_DIR)/client/imgui_impl_raylib.o \
	$(WINDOWS_BUILD_DIR)/client/imgui_wrapper.o
MACOS_IMGUI_OBJECTS := $(MACOS_BUILD_DIR)/client/imgui_impl_raylib.o \
	$(MACOS_BUILD_DIR)/client/imgui_wrapper.o

#Client source files
CLIENT_SOURCES := \
	$(CLIENT_SRC_DIR)/main.c \
	$(CLIENT_SRC_DIR)/audio.c \
	$(CLIENT_SRC_DIR)/client_settings.c \
	$(CLIENT_SRC_DIR)/game.c \
	$(CLIENT_SRC_DIR)/layout.c \
	$(CLIENT_SRC_DIR)/net.c \
	$(CLIENT_SRC_DIR)/screen.c \
	$(CLIENT_SRC_DIR)/screens/screen_background.c \
	$(CLIENT_SRC_DIR)/screens/main_menu.c \
	$(CLIENT_SRC_DIR)/screens/settings.c \
	$(CLIENT_SRC_DIR)/screens/help.c \
	$(CLIENT_SRC_DIR)/screens/credits.c \
	$(CLIENT_SRC_DIR)/screens/server_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_roster.c \
	$(CLIENT_SRC_DIR)/screens/gameplay.c \
	$(CLIENT_SRC_DIR)/screens/results.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/connection.c

#Server source files
SERVER_SOURCES := \
	$(SERVER_SRC_DIR)/main.c \
	$(SERVER_SRC_DIR)/logger.c \
	$(SERVER_SRC_DIR)/database.c \
	$(SERVER_SRC_DIR)/auth.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/connection.c

#Shared headers(dependencies for all modules)
SHARED_HEADERS := \
	$(SHARED_SRC_DIR)/config.h \
	$(SHARED_SRC_DIR)/vec2.h \
	$(SHARED_SRC_DIR)/world.h \
	$(SHARED_SRC_DIR)/sim.h \
	$(SHARED_SRC_DIR)/protocol.h \
	$(SHARED_SRC_DIR)/profiler.h \
	$(SHARED_SRC_DIR)/lifecycle.h \
	$(SHARED_SRC_DIR)/connection.h \
	$(CLIENT_SRC_DIR)/audio.h \
	$(CLIENT_SRC_DIR)/layout.h \
	$(SERVER_SRC_DIR)/database.h \
	$(SERVER_SRC_DIR)/auth.h

#Object files
CLIENT_LINUX_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
CLIENT_WINDOWS_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(WINDOWS_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
CLIENT_MACOS_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(MACOS_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
SERVER_LINUX_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(SERVER_SOURCES))
SERVER_WINDOWS_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(WINDOWS_BUILD_DIR)/%.o,$(SERVER_SOURCES))
SERVER_MACOS_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(MACOS_BUILD_DIR)/%.o,$(SERVER_SOURCES))

#Test files
TEST_SRCS := $(wildcard $(UNIT_TESTS_DIR)/*.c)
TEST_BINS := $(patsubst $(UNIT_TESTS_DIR)/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))
IMGUI_CORE_SOURCES := imgui imgui_draw imgui_tables imgui_widgets
IMGUI_TEST_ENGINE_SOURCE_NAMES := imgui_capture_tool imgui_te_context imgui_te_coroutine \
	imgui_te_engine imgui_te_exporters imgui_te_perftool imgui_te_ui imgui_te_utils
IMGUI_TEST_CLIENT_SOURCES := \
	$(CLIENT_SRC_DIR)/audio.c \
	$(CLIENT_SRC_DIR)/client_settings.c \
	$(CLIENT_SRC_DIR)/game.c \
	$(CLIENT_SRC_DIR)/layout.c \
	$(CLIENT_SRC_DIR)/net.c \
	$(CLIENT_SRC_DIR)/screen.c \
	$(CLIENT_SRC_DIR)/screens/screen_background.c \
	$(CLIENT_SRC_DIR)/screens/main_menu.c \
	$(CLIENT_SRC_DIR)/screens/settings.c \
	$(CLIENT_SRC_DIR)/screens/help.c \
	$(CLIENT_SRC_DIR)/screens/credits.c \
	$(CLIENT_SRC_DIR)/screens/server_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_roster.c \
	$(CLIENT_SRC_DIR)/screens/gameplay.c \
	$(CLIENT_SRC_DIR)/screens/results.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/connection.c
# C test driver sources (plain C11 — no C++ in the test logic)
IMGUI_TEST_C_SOURCES := $(IMGUI_TESTS_DIR)/main.c $(IMGUI_TESTS_DIR)/tests.c
# C++ wrapper sources for the test harness (test engine C API bridge)
IMGUI_TEST_CPP_WRAPPER_SOURCES := $(IMGUI_TESTS_DIR)/imgui_te_wrapper.cpp
IMGUI_TEST_BIN := $(TEST_BUILD_DIR)/imgui/shroomio-imgui-tests
IMGUI_TEST_CLIENT_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(TEST_BUILD_DIR)/imgui/%.o,$(IMGUI_TEST_CLIENT_SOURCES))
IMGUI_TEST_C_OBJECTS := $(patsubst $(TESTS_DIR)/imgui/%.c,$(TEST_BUILD_DIR)/imgui/tests/%.o,$(IMGUI_TEST_C_SOURCES))
IMGUI_TEST_CPP_WRAPPER_OBJECTS := $(patsubst $(TESTS_DIR)/imgui/%.cpp,$(TEST_BUILD_DIR)/imgui/tests/%.o,$(IMGUI_TEST_CPP_WRAPPER_SOURCES))
IMGUI_TEST_IMGUI_OBJECTS := $(addprefix $(TEST_BUILD_DIR)/imgui/imgui/,$(addsuffix .o,$(IMGUI_CORE_SOURCES))) \
	$(TEST_BUILD_DIR)/imgui/client/imgui_impl_raylib.o \
	$(TEST_BUILD_DIR)/imgui/client/imgui_wrapper.o
IMGUI_TEST_ENGINE_OBJECTS := $(addprefix $(TEST_BUILD_DIR)/imgui/engine/,$(addsuffix .o,$(IMGUI_TEST_ENGINE_SOURCE_NAMES)))

# =============================================================================
# 5. Build Targets
# =============================================================================
.PHONY: all client-linux client-windows client-macos server-linux server-windows server-macos servers-all
.PHONY: linux windows macos server run run-server run-windows help
.PHONY: benchmark

all: client-linux

help:
	@echo ""
	@echo "shroomio build system"
	@echo "====================="
	@echo ""
	@echo "Build targets:"
	@echo "  make client-linux   Build the Linux client binary"
	@echo "  make client-windows Build the Windows client binary (requires mingw-w64)"
	@echo "  make client-macos   Build the macOS client binary"
	@echo "  make server-linux   Build the Linux headless server binary"
	@echo "  make server-windows Build the Windows headless server binary (requires mingw-w64)"
	@echo "  make server-macos   Build the macOS headless server binary"
	@echo "  make servers-all    Build all headless server binaries (Linux, Windows, macOS)"
	@echo "  make linux/windows/macos/server  Compatibility aliases"
	@echo "  make run            Build and run the Linux client"
	@echo "  make run-server     Build and run the Linux server"
	@echo "  make benchmark      Run repeatable local server benchmark scenarios"
	@echo "  make run-windows    Build and run the Windows client (via WSL)"
	@echo ""
	@echo "Quality targets:"
	@echo "  make test           Run unit tests + ImGui tests"
	@echo "  make imgui-test     Run ImGui screen tests"
	@echo "  make test-coverage  Run tests with coverage report"
	@echo "  make valgrind-test  Run unit tests and server smoke test under Valgrind"
	@echo "  make lint           Run all linters (format-check + cppcheck)"
	@echo "  make format-check   Check code formatting (clang-format)"
	@echo "  make format         Auto-format code (clang-format -i)"
	@echo "  make check          Run lint + test (CI validation)"
	@echo ""
	@echo "Docker targets:"
	@echo "  make docker-server        Build the server container image"
	@echo "  make docker-run-server    Build and run the server container"
	@echo "  make docker-logs          Follow the server container logs"
	@echo ""
	@echo "Devcontainer targets:"
	@echo "  make devcontainer-build         Build the dev container image"
	@echo "  make devcontainer-up            Start the dev container"
	@echo "  make devcontainer-shell         Open a shell in the dev container"
	@echo "  make devcontainer-exec CMD=...  Run a command in the dev container"
	@echo "  make devcontainer-gh ARGS=...   Run gh inside the dev container"
	@echo "  make devcontainer-down          Stop the dev container"
	@echo "  make devcontainer-opencode-sync Refresh OpenCode state"
	@echo "  make devcontainer-git-identity  Copy host git identity"
	@echo "  make devcontainer-github-token  Store GitHub token"
	@echo "  make devcontainer-github-status Check GitHub auth"
	@echo ""
	@echo "vcpkg dependencies:"
	@echo "  make vcpkg-bootstrap      Bootstrap the local vcpkg tool"
	@echo "  make vcpkg-install-linux  Install Linux vcpkg dependencies"
	@echo "  make vcpkg-install-windows Install Windows vcpkg dependencies"
	@echo "  make vcpkg-install-macos  Install macOS vcpkg dependencies"
	@echo "  make vcpkg-install        Install Linux and Windows vcpkg dependencies"
	@echo ""
	@echo "Documentation:"
	@echo "  make spec           Build the LaTeX specification PDF"
	@echo ""
	@echo "Manual test dependencies:"
	@echo "  make vendor         Download Unity, test-only ImGui, and ImGui Test Engine"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean          Remove build and dist artifacts"
	@echo "  make distclean      Remove build artifacts and vendor sources"
	@echo "  make test-clean     Remove test build artifacts"
	@echo ""

client-linux: $(CLIENT_LINUX_BIN)

client-windows: $(CLIENT_WINDOWS_BIN)

client-macos: $(CLIENT_MACOS_BIN)

server-linux: $(SERVER_LINUX_BIN)

server-windows: $(SERVER_WINDOWS_BIN)

server-macos: $(SERVER_MACOS_BIN)

servers-all: server-linux server-windows server-macos

linux: client-linux

windows: client-windows

macos: client-macos

server: server-linux

run: $(CLIENT_LINUX_BIN)
	./$(CLIENT_LINUX_BIN)

run-server: $(SERVER_LINUX_BIN)
	./$(SERVER_LINUX_BIN)

benchmark: $(SERVER_LINUX_BIN)
	python3 scripts/benchmark.py --server ./$(SERVER_LINUX_BIN)

run-windows: $(CLIENT_WINDOWS_BIN)
	./$(CLIENT_WINDOWS_BIN)

# =============================================================================
# 6. Compilation Rules
# =============================================================================

# Link targets
$(CLIENT_LINUX_BIN): $(CLIENT_LINUX_OBJECTS) $(LINUX_IMGUI_OBJECTS) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(CLIENT_LINUX_OBJECTS) $(LINUX_IMGUI_OBJECTS) -o $@ $(LINUX_THIRD_PARTY_LIBS) $(LINUX_LIBS)

$(CLIENT_WINDOWS_BIN): $(CLIENT_WINDOWS_OBJECTS) $(WINDOWS_IMGUI_OBJECTS) $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CXX) -static $(CLIENT_WINDOWS_OBJECTS) $(WINDOWS_IMGUI_OBJECTS) -o $@ $(WINDOWS_THIRD_PARTY_LIBS) $(WINDOWS_LIBS)

$(CLIENT_MACOS_BIN): $(CLIENT_MACOS_OBJECTS) $(MACOS_IMGUI_OBJECTS) $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CXX) $(CLIENT_MACOS_OBJECTS) $(MACOS_IMGUI_OBJECTS) -o $@ $(MACOS_THIRD_PARTY_LIBS) $(MACOS_LIBS)

$(SERVER_LINUX_BIN): $(SERVER_LINUX_OBJECTS) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_LINUX_OBJECTS) -o $@ $(LINUX_SERVER_LIBS)

$(SERVER_WINDOWS_BIN): $(SERVER_WINDOWS_OBJECTS) $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) -static $(SERVER_WINDOWS_OBJECTS) -o $@ $(WINDOWS_SERVER_LIBS)

$(SERVER_MACOS_BIN): $(SERVER_MACOS_OBJECTS) $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CC) $(SERVER_MACOS_OBJECTS) -o $@ $(MACOS_SERVER_LIBS)

# Client object compilation
$(LINUX_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_CFLAGS) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE -c $< -o $@

$(WINDOWS_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) | $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_CFLAGS) -c $< -o $@

$(MACOS_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) | $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CC) $(MACOS_CFLAGS) -c $< -o $@

$(LINUX_BUILD_DIR)/client/imgui_impl_raylib.o: $(CLIENT_SRC_DIR)/imgui_impl_raylib.cpp | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) -c $< -o $@

$(LINUX_BUILD_DIR)/client/imgui_wrapper.o: $(CLIENT_SRC_DIR)/imgui_wrapper.cpp | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/client/imgui_impl_raylib.o: $(CLIENT_SRC_DIR)/imgui_impl_raylib.cpp | $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CXX) $(WINDOWS_CXXFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/client/imgui_wrapper.o: $(CLIENT_SRC_DIR)/imgui_wrapper.cpp | $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CXX) $(WINDOWS_CXXFLAGS) -c $< -o $@

$(MACOS_BUILD_DIR)/client/imgui_impl_raylib.o: $(CLIENT_SRC_DIR)/imgui_impl_raylib.cpp | $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CXX) $(MACOS_CXXFLAGS) -c $< -o $@

$(MACOS_BUILD_DIR)/client/imgui_wrapper.o: $(CLIENT_SRC_DIR)/imgui_wrapper.cpp | $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CXX) $(MACOS_CXXFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(IMGUI_TEST_CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(IMGUI_TEST_CFLAGS) -c $< -o $@

# C++ wrapper compilation (imgui_te_wrapper.cpp and any future .cpp files in tests/imgui/)
$(TEST_BUILD_DIR)/imgui/tests/%.o: $(IMGUI_TESTS_DIR)/%.cpp $(IMGUI_TEST_ENGINE_DIR) $(IMGUI_DIR) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_CXXFLAGS) -I$(IMGUI_TESTS_DIR) -c $< -o $@

# C test driver compilation (main.c, tests.c) — depends on game.h/net.h so
# layout changes automatically rebuild test objects.
$(TEST_BUILD_DIR)/imgui/tests/%.o: $(IMGUI_TESTS_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(IMGUI_TEST_CFLAGS) -I$(IMGUI_TESTS_DIR) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/imgui/%.o: $(IMGUI_SRC_DIR)/%.cpp $(IMGUI_DIR) $(IMGUI_TEST_ENGINE_DIR) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_CXXFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/client/imgui_impl_raylib.o: $(CLIENT_SRC_DIR)/imgui_impl_raylib.cpp $(IMGUI_DIR) $(IMGUI_TEST_ENGINE_DIR) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_CXXFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/client/imgui_wrapper.o: $(CLIENT_SRC_DIR)/imgui_wrapper.cpp $(IMGUI_DIR) $(IMGUI_TEST_ENGINE_DIR) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_CXXFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/imgui/engine/%.o: $(IMGUI_TEST_ENGINE_SRC_DIR)/%.cpp $(IMGUI_TEST_ENGINE_DIR) $(IMGUI_DIR) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_ENGINE_CXXFLAGS) -c $< -o $@

# Shared object compilation
$(LINUX_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS) | $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_SERVER_CFLAGS) -c $< -o $@

$(MACOS_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS) | $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CC) $(MACOS_CFLAGS) -c $< -o $@

# Server object compilation
$(LINUX_BUILD_DIR)/server/%.o: $(SERVER_SRC_DIR)/%.c | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/server/%.o: $(SERVER_SRC_DIR)/%.c | $(VCPKG_WINDOWS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_SERVER_CFLAGS) -c $< -o $@

$(MACOS_BUILD_DIR)/server/%.o: $(SERVER_SRC_DIR)/%.c | $(VCPKG_MACOS_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(MACOS_CC) $(MACOS_SERVER_CFLAGS) -c $< -o $@

# =============================================================================
# 7. Dependency Installation
# =============================================================================
.PHONY: vendor vcpkg-bootstrap vcpkg-install vcpkg-install-linux vcpkg-install-windows vcpkg-install-macos

vcpkg-bootstrap:
	@test -x "$(VCPKG_BIN)" || test ! -f "$(VCPKG_ROOT)/bootstrap-vcpkg.sh" || bash "$(VCPKG_ROOT)/bootstrap-vcpkg.sh" -disableMetrics

$(VCPKG_LINUX_STAMP): vcpkg.json | vcpkg-bootstrap
	@$(MKDIR_P) $(VCPKG_LINUX_INSTALLED_DIR)
	$(VCPKG_BIN) install --triplet $(VCPKG_LINUX_TRIPLET) --x-manifest-root=$(CURDIR) --x-install-root=$(VCPKG_LINUX_INSTALLED_DIR)
	@touch $@

$(VCPKG_WINDOWS_STAMP): vcpkg.json | vcpkg-bootstrap
	@$(MKDIR_P) $(VCPKG_WINDOWS_INSTALLED_DIR)
	$(VCPKG_BIN) install --triplet $(VCPKG_WINDOWS_TRIPLET) --x-manifest-root=$(CURDIR) --x-install-root=$(VCPKG_WINDOWS_INSTALLED_DIR)
	@touch $@

$(VCPKG_MACOS_STAMP): vcpkg.json | vcpkg-bootstrap
	@$(MKDIR_P) $(VCPKG_MACOS_INSTALLED_DIR)
	$(VCPKG_BIN) install --triplet $(VCPKG_MACOS_TRIPLET) --x-manifest-root=$(CURDIR) --x-install-root=$(VCPKG_MACOS_INSTALLED_DIR)
	@touch $@

vcpkg-install-linux: $(VCPKG_LINUX_STAMP)

vcpkg-install-windows: $(VCPKG_WINDOWS_STAMP)

vcpkg-install-macos: $(VCPKG_MACOS_STAMP)

vcpkg-install: $(VCPKG_LINUX_STAMP) $(VCPKG_WINDOWS_STAMP)

vendor: $(IMGUI_DIR) $(IMGUI_TEST_ENGINE_DIR) $(UNITY_DIR)

$(IMGUI_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(IMGUI_URL) -o $(BUILD_DIR)/imgui.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/imgui.tar.gz -C vendor

$(IMGUI_TEST_ENGINE_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(IMGUI_TEST_ENGINE_URL) -o $(BUILD_DIR)/imgui_test_engine.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/imgui_test_engine.tar.gz -C vendor

$(UNITY_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(UNITY_URL) -o $(BUILD_DIR)/unity.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/unity.tar.gz -C vendor

# =============================================================================
# 8. Test Targets
# =============================================================================
.PHONY: test unit-test imgui-test valgrind-test valgrind-unit-test valgrind-server-smoke valgrind-imgui-test test-coverage test-clean

test: unit-test imgui-test

unit-test: $(TEST_BINS)
	@echo "Running unit tests..."
	@failed=0; total=0; \
	for test in $(TEST_BINS); do \
		total=$$((total + 1)); \
		echo ""; \
		echo "=== Running $$(basename $$test) ==="; \
		if $$test; then \
			echo "✓ $$(basename $$test) passed"; \
		else \
			echo "✗ $$(basename $$test) failed"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "=== Test Summary ==="; \
	echo "Total: $$total, Passed: $$((total - failed)), Failed: $$failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi

$(IMGUI_TEST_BIN): $(IMGUI_TEST_CLIENT_OBJECTS) $(IMGUI_TEST_C_OBJECTS) $(IMGUI_TEST_CPP_WRAPPER_OBJECTS) $(IMGUI_TEST_IMGUI_OBJECTS) $(IMGUI_TEST_ENGINE_OBJECTS) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CXX) $(IMGUI_TEST_CLIENT_OBJECTS) $(IMGUI_TEST_C_OBJECTS) $(IMGUI_TEST_CPP_WRAPPER_OBJECTS) $(IMGUI_TEST_IMGUI_OBJECTS) $(IMGUI_TEST_ENGINE_OBJECTS) -o $@ $(IMGUI_TEST_THIRD_PARTY_LIBS) $(LINUX_LIBS) -pthread

imgui-test: $(IMGUI_TEST_BIN)
	@echo "Running ImGui tests..."
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a ./$(IMGUI_TEST_BIN); else ./$(IMGUI_TEST_BIN); fi

VALGRIND ?= valgrind
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=99 --track-origins=yes
VALGRIND_IMGUI_FLAGS ?= --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=none --error-exitcode=99 --track-origins=yes
VALGRIND_IMGUI_TEST_FILTERS ?=

valgrind-test: valgrind-unit-test valgrind-server-smoke

valgrind-unit-test: $(TEST_BINS)
	@command -v $(VALGRIND) >/dev/null 2>&1 || (printf '%s\n' 'valgrind is not installed. Rebuild the devcontainer with make devcontainer-build && make devcontainer-up.' && exit 1)
	@echo "Running unit tests under Valgrind..."
	@failed=0; total=0; \
	for test in $(TEST_BINS); do \
		total=$$((total + 1)); \
		echo ""; \
		echo "=== Valgrind $$(basename $$test) ==="; \
		if SHROOM_VALGRIND=1 $(VALGRIND) $(VALGRIND_FLAGS) $$test; then \
			echo "✓ $$(basename $$test) passed under Valgrind"; \
		else \
			echo "✗ $$(basename $$test) failed under Valgrind"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "=== Valgrind Test Summary ==="; \
	echo "Total: $$total, Passed: $$((total - failed)), Failed: $$failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi

valgrind-server-smoke: $(SERVER_LINUX_BIN)
	@command -v $(VALGRIND) >/dev/null 2>&1 || (printf '%s\n' 'valgrind is not installed. Rebuild the devcontainer with make devcontainer-build && make devcontainer-up.' && exit 1)
	@echo "Running server startup/shutdown smoke test under Valgrind..."
	@rm -f /tmp/shroomio-valgrind-server.db /tmp/shroomio-valgrind-server.db-journal
	@$(VALGRIND) $(VALGRIND_FLAGS) ./$(SERVER_LINUX_BIN) --smoke-test --bind 127.0.0.1 --port 37777 --database /tmp/shroomio-valgrind-server.db
	@rm -f /tmp/shroomio-valgrind-server.db /tmp/shroomio-valgrind-server.db-journal

valgrind-imgui-test: $(IMGUI_TEST_BIN)
	@command -v $(VALGRIND) >/dev/null 2>&1 || (printf '%s\n' 'valgrind is not installed. Rebuild the devcontainer with make devcontainer-build && make devcontainer-up.' && exit 1)
	@echo "Running ImGui tests under Valgrind..."
	@if [ -z "$(strip $(VALGRIND_IMGUI_TEST_FILTERS))" ]; then \
		if command -v xvfb-run >/dev/null 2>&1; then \
			SHROOM_VALGRIND_IMGUI=1 xvfb-run -a $(VALGRIND) $(VALGRIND_IMGUI_FLAGS) ./$(IMGUI_TEST_BIN); \
		else \
			SHROOM_VALGRIND_IMGUI=1 $(VALGRIND) $(VALGRIND_IMGUI_FLAGS) ./$(IMGUI_TEST_BIN); \
		fi; \
	else \
		failed=0; total=0; \
		for filter in $(VALGRIND_IMGUI_TEST_FILTERS); do \
			total=$$((total + 1)); \
			echo ""; \
			echo "=== Valgrind ImGui $$filter ==="; \
			if command -v xvfb-run >/dev/null 2>&1; then \
				SHROOM_VALGRIND_IMGUI=1 SHROOM_IMGUI_TEST_FILTER=$$filter xvfb-run -a $(VALGRIND) $(VALGRIND_IMGUI_FLAGS) ./$(IMGUI_TEST_BIN); \
			else \
				SHROOM_VALGRIND_IMGUI=1 SHROOM_IMGUI_TEST_FILTER=$$filter $(VALGRIND) $(VALGRIND_IMGUI_FLAGS) ./$(IMGUI_TEST_BIN); \
			fi; \
			if [ $$? -ne 0 ]; then failed=$$((failed + 1)); fi; \
		done; \
		echo ""; \
		echo "=== Valgrind ImGui Summary ==="; \
		echo "Total: $$total, Passed: $$((total - failed)), Failed: $$failed"; \
		if [ $$failed -gt 0 ]; then exit 1; fi; \
	fi

# Test with coverage (requires gcovr)
test-coverage: $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@command -v gcovr >/dev/null 2>&1 || (printf '%s\n' 'gcovr is not installed. Install gcovr or run this target inside the devcontainer: make devcontainer-exec CMD="make test-coverage".' && exit 1)
	@echo "Building tests with coverage instrumentation..."
	@$(MKDIR_P) $(TEST_BUILD_DIR) $(COVERAGE_DIR)
	@for src in $(TEST_SRCS); do \
		test_name=$$(basename $$src .c); \
		echo "Compiling $$test_name..."; \
	done
	@set -e; failed=0; total=0; \
	for src in $(TEST_SRCS); do \
		test_name=$$(basename $$src .c); \
		test_bin=$(TEST_BUILD_DIR)/$$test_name; \
		total=$$((total + 1)); \
		case $$test_name in \
			test_auth) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/auth.c $(SERVER_SRC_DIR)/logger.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3 ;; \
			test_lifecycle) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_screen) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_screen_background) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/screens/screen_background.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_connection) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_client_net) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/net.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lenet ;; \
			test_sim) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			*) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) -o $$test_bin $(COVERAGE_LIBS) ;; \
		esac; \
		echo ""; \
		echo "=== Running $$test_name ==="; \
		if $$test_bin; then \
			echo "✓ $$test_name passed"; \
		else \
			echo "✗ $$test_name failed"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "=== Test Summary ==="; \
	echo "Total: $$total, Passed: $$((total - failed)), Failed: $$failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "Generating coverage report..."
	@gcovr -r . --html --html-details -o $(COVERAGE_DIR)/index.html
	@echo "Coverage report generated: $(COVERAGE_DIR)/index.html"

test-clean:
	$(RM_RF) $(TEST_BUILD_DIR) $(COVERAGE_DIR) *.gcda *.gcno

# Specific test targets with explicit dependencies
$(TEST_BUILD_DIR)/test_lifecycle: $(UNIT_TESTS_DIR)/test_lifecycle.c $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_screen: $(UNIT_TESTS_DIR)/test_screen.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_screen_background: $(UNIT_TESTS_DIR)/test_screen_background.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/screens/screen_background.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_connection: $(UNIT_TESTS_DIR)/test_connection.c $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_audio: $(UNIT_TESTS_DIR)/test_audio.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/audio.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) $(LINUX_THIRD_PARTY_LIBS) $(LINUX_LIBS)

$(TEST_BUILD_DIR)/test_client_budget: $(UNIT_TESTS_DIR)/test_client_budget.c $(UNITY_SRC) | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_client_net: $(UNIT_TESTS_DIR)/test_client_net.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/net.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(TEST_BUILD_DIR)/test_sim: $(UNIT_TESTS_DIR)/test_sim.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_auth: $(UNIT_TESTS_DIR)/test_auth.c $(UNITY_SRC) $(SERVER_SRC_DIR)/auth.c $(SERVER_SRC_DIR)/logger.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3

$(TEST_BUILD_DIR)/%: $(UNIT_TESTS_DIR)/%.c $(UNITY_SRC) | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

# =============================================================================
# 9. Docker Targets
# =============================================================================
.PHONY: docker-server docker-run-server docker-logs
.PHONY: devcontainer-build devcontainer-up devcontainer-shell devcontainer-exec devcontainer-gh devcontainer-down
.PHONY: devcontainer-opencode-sync devcontainer-git-identity devcontainer-github-token devcontainer-github-status

# Server container settings
SERVER_IMAGE ?= shroomio-server:dev
SERVER_CONTAINER ?= shroomio-server-1

# Devcontainer settings
DEVCONTAINER_IMAGE ?= shroomio-devcontainer:dev
DEVCONTAINER_CONTAINER ?= shroomio-devcontainer
DEVCONTAINER_SECRET_DIR ?= $(HOME)/.config/shroomio-devcontainer
DEVCONTAINER_GITHUB_TOKEN_FILE ?= $(DEVCONTAINER_SECRET_DIR)/github-token
DOCKER_SOCK_GID := $(shell stat -c '%g' /var/run/docker.sock 2>/dev/null || printf '0')
GIT_USER_NAME := $(shell git config --global --get user.name)
GIT_USER_EMAIL := $(shell git config --global --get user.email)

# Server Docker targets
docker-server: $(VCPKG_LINUX_STAMP)
	$(DOCKER) build -f Dockerfile.server -t $(SERVER_IMAGE) .

docker-run-server: docker-server
	$(DOCKER) run --rm -p 7777:7777/udp $(SERVER_IMAGE)

docker-logs:
	$(DOCKER) logs -f $(SERVER_CONTAINER)

# Devcontainer targets
devcontainer-build:
	DOCKER_BUILDKIT=0 $(DOCKER) build -f .devcontainer/Dockerfile -t $(DEVCONTAINER_IMAGE) .

devcontainer-up:
	@mkdir -p "$(DEVCONTAINER_SECRET_DIR)"
	@touch "$(DEVCONTAINER_GITHUB_TOKEN_FILE)"
	@chmod 600 "$(DEVCONTAINER_GITHUB_TOKEN_FILE)"
	$(DOCKER) rm -f $(DEVCONTAINER_CONTAINER) >/dev/null 2>&1 || true
	$(DOCKER) run -d --name $(DEVCONTAINER_CONTAINER) \
		-u dev \
		--group-add $(DOCKER_SOCK_GID) \
		-v "$(PWD):/workspaces/shroomio" \
		-v /var/run/docker.sock:/var/run/docker.sock \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-v "$(HOME)/.ssh:/home/dev/.ssh:ro" \
		-v "$(HOME)/.gitconfig:/home/dev/.gitconfig:ro" \
		-v "$(DEVCONTAINER_GITHUB_TOKEN_FILE):/run/secrets/shroomio-github-token:ro" \
		-v "$(HOME)/.config/opencode:/mnt/host-opencode/config:ro" \
		-v "$(HOME)/.local/share/opencode:/mnt/host-opencode/share:ro" \
		-v "$(HOME)/.local/state/opencode:/mnt/host-opencode/state:ro" \
		-v "$(HOME)/.cache/opencode:/mnt/host-opencode/cache:ro" \
		-e HOME="/home/dev" \
		-e GIT_USER_NAME="$(GIT_USER_NAME)" \
		-e GIT_USER_EMAIL="$(GIT_USER_EMAIL)" \
		-e DISPLAY="$(DISPLAY)" \
		-e WAYLAND_DISPLAY="$(WAYLAND_DISPLAY)" \
		-e XDG_RUNTIME_DIR="$(XDG_RUNTIME_DIR)" \
		-w /workspaces/shroomio \
		$(DEVCONTAINER_IMAGE) bash -lc 'bash .devcontainer/init-opencode.sh && bash .devcontainer/init-git-identity.sh && bash .devcontainer/init-github-auth.sh && sleep infinity'

devcontainer-shell:
	$(DOCKER) exec -it \
		-u dev \
		-e HOME=/home/dev \
		-e TERM="$(TERM)" \
		$(DEVCONTAINER_CONTAINER) bash -il

devcontainer-exec:
	@test -n "$(CMD)" || (printf '%s\n' 'Usage: make devcontainer-exec CMD="gh issue list"' && exit 1)
	$(DOCKER) exec -u dev -e HOME=/home/dev -e TERM="$(TERM)" $(DEVCONTAINER_CONTAINER) bash -lc "$(CMD)"

devcontainer-gh:
	$(DOCKER) exec -u dev -e HOME=/home/dev -e TERM="$(TERM)" $(DEVCONTAINER_CONTAINER) gh $(ARGS)

devcontainer-down:
	$(DOCKER) rm -f $(DEVCONTAINER_CONTAINER)

devcontainer-opencode-sync:
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) bash -lc 'bash .devcontainer/init-opencode.sh'

devcontainer-git-identity:
	@test -n "$(GIT_USER_NAME)" || (printf '%s\n' 'Host git user.name is not set.' && exit 1)
	@test -n "$(GIT_USER_EMAIL)" || (printf '%s\n' 'Host git user.email is not set.' && exit 1)
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) git config --global user.name "$(GIT_USER_NAME)"
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) git config --global user.email "$(GIT_USER_EMAIL)"
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) git config --global --get user.name
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) git config --global --get user.email

devcontainer-github-token:
	@mkdir -p "$(DEVCONTAINER_SECRET_DIR)"
	@printf '%s\n' 'Paste your GitHub token. Input will be hidden.'
	@bash -lc 'read -rsp "Token: " token; printf "\n"; umask 177; printf "%s" "$$token" > "$(DEVCONTAINER_GITHUB_TOKEN_FILE)"'
	@printf '%s\n' "Saved token to $(DEVCONTAINER_GITHUB_TOKEN_FILE)"

devcontainer-github-status:
	$(DOCKER) exec -u dev -e HOME=/home/dev $(DEVCONTAINER_CONTAINER) gh auth status

# =============================================================================
# 10. Lint & Format Targets
# =============================================================================
.PHONY: lint format format-check check

# Source files for linting
LINT_SOURCES := $(shell find src -name '*.c' -o -name '*.h')

# Check code formatting (CI uses this)
format-check:
	@echo "Checking code formatting..."
	@echo $(LINT_SOURCES) | xargs clang-format --dry-run --Werror

# Auto-format code
format:
	@echo "Formatting code..."
	@echo $(LINT_SOURCES) | xargs clang-format -i
	@echo "Done."

# Run cppcheck static analysis
cppcheck:
	@echo "Running cppcheck..."
	cppcheck --enable=warning,style,performance,portability \
		--error-exitcode=1 \
		--include=src/shared/config.h \
		src/

# Run all linters
lint: format-check cppcheck
	@echo "All lint checks passed."

# Run full CI validation locally
check: lint test
	@echo "All checks passed."

# =============================================================================
# 11. Documentation
# =============================================================================
.PHONY: spec

SPEC_SRC := design/shroomio-specification.tex
SPEC_OUT := dist/latex

spec:
	@$(MKDIR_P) $(SPEC_OUT)
	latexmk -pdf -outdir=$(SPEC_OUT) $(SPEC_SRC)

# =============================================================================
# 12. Cleanup
# =============================================================================
.PHONY: clean distclean

clean:
	$(RM_RF) $(BUILD_DIR) $(DIST_DIR)

distclean: clean
	$(RM_RF) vendor vcpkg_installed
