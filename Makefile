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
NETWORK_BENCH_BIN := $(BUILD_DIR)/benchmarks/network-benchmark
SERVER_HEALTHCHECK_BIN := $(BUILD_DIR)/tools/shroomio-healthcheck
CLIENT_REST_SMOKE_BIN := $(BUILD_DIR)/tools/shroomio-client-rest-smoke
INPUT_FLOOD_CLIENT_BIN := $(BUILD_DIR)/tests/input-flood-client
DIRECTORY_QUERY_CLIENT_BIN := $(BUILD_DIR)/tests/directory-query-client
SERVER_DISCOVERY_CLIENT_BIN := $(BUILD_DIR)/tests/server-discovery-client
QUICK_MATCH_CLIENT_BIN := $(BUILD_DIR)/tests/quick-match-client
SNAPSHOT_RATE_PROBE_BIN := $(BUILD_DIR)/tests/snapshot-rate-probe
GRACEFUL_SHUTDOWN_CLIENT_BIN := $(BUILD_DIR)/tests/graceful-shutdown-client
UDP_AUTH_COLLISION_CLIENT_BIN := $(BUILD_DIR)/tests/udp-auth-collision-client
INPUT_FLOOD_PORT ?=
NETWORK_BENCH_CLIENTS ?= 1,64,256
NETWORK_BENCH_DURATION_MS ?= 1500
NETWORK_BENCH_SPLIT_PIECES ?= 1

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
WINDOWS_CFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER -DCURL_STATICLIB
MACOS_CFLAGS   := $(COMMON_CFLAGS) -I$(VCPKG_MACOS_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
LINUX_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
WINDOWS_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_WINDOWS_INCLUDE_DIR) -DPLATFORM_DESKTOP
WINDOWS_CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER -DCURL_STATICLIB
MACOS_CXXFLAGS := $(COMMON_CXXFLAGS) -I$(VCPKG_MACOS_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE

#Server compiler flags(headless, ENet - based)
LINUX_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_LINUX_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
WINDOWS_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_WINDOWS_INCLUDE_DIR)
WINDOWS_SERVER_CFLAGS += -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER
MACOS_SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
		  -I$(VCPKG_MACOS_INCLUDE_DIR) -D_DEFAULT_SOURCE
LINUX_SERVER_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -lenet -lcivetweb -largon2 -lcjson -lssl -lcrypto -lz -lm \
	-lsqlite3 -ldl -lpthread
WINDOWS_SERVER_LIBS := -L$(VCPKG_WINDOWS_LIB_DIR) -lenet -lcivetweb -largon2 -lcjson -lssl -lcrypto -lzs \
	-lsqlite3 -lws2_32 -lwinmm -lcrypt32 -lgdi32 -lwinpthread
MACOS_SERVER_LIBS := -L$(VCPKG_MACOS_LIB_DIR) -lenet -lcivetweb -largon2 -lcjson -lssl -lcrypto -lz \
	-lsqlite3 -lm -lpthread

#Test compiler flags(UNITY_INCLUDE defined in vendor section)
TEST_LIBS := -lm

#Platform link libraries
LINUX_LIBS   := -lGL -lm -ldl -lpthread -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lasound
WINDOWS_LIBS := -lopengl32 -lgdi32 -lwinmm -lbcrypt -ladvapi32 -lcrypt32 -lws2_32 -liphlpapi \
	-lshell32
MACOS_LIBS   := -lm -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
	-framework Security -framework SystemConfiguration
LINUX_THIRD_PARTY_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -limgui -lenet -lopus -lraylib -lglfw3 \
	-lcurl -lcjson -lssl -lcrypto -lz
WINDOWS_THIRD_PARTY_LIBS := -L$(VCPKG_WINDOWS_LIB_DIR) -limgui -lenet -lopus -lraylib -lglfw3 \
	-lcurl -lcjson -lssl -lcrypto -lzs
MACOS_THIRD_PARTY_LIBS := -L$(VCPKG_MACOS_LIB_DIR) -limgui -lenet -lopus -lraylib -lglfw3 \
	-lcurl -lcjson -lssl -lcrypto -lz
IMGUI_TEST_THIRD_PARTY_LIBS := -L$(VCPKG_LINUX_LIB_DIR) -lenet -lopus -lraylib -lglfw3 \
	-lcjson -lcrypto

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
	$(CLIENT_SRC_DIR)/account_flow.c \
	$(CLIENT_SRC_DIR)/chat_cache.c \
	$(CLIENT_SRC_DIR)/audio.c \
	$(CLIENT_SRC_DIR)/client_settings.c \
	$(CLIENT_SRC_DIR)/client_rest.c \
	$(CLIENT_SRC_DIR)/client_rest_curl.c \
	$(CLIENT_SRC_DIR)/client_session_store.c \
	$(CLIENT_SRC_DIR)/cursor.c \
	$(CLIENT_SRC_DIR)/game.c \
	$(CLIENT_SRC_DIR)/game_mode_availability.c \
	$(CLIENT_SRC_DIR)/input_scheduler.c \
	$(CLIENT_SRC_DIR)/layout.c \
	$(CLIENT_SRC_DIR)/layout_metrics.c \
	$(CLIENT_SRC_DIR)/matchmaking_selector.c \
	$(CLIENT_SRC_DIR)/match_feedback.c \
	$(CLIENT_SRC_DIR)/match_presentation.c \
	$(CLIENT_SRC_DIR)/net.c \
	$(CLIENT_SRC_DIR)/prediction.c \
	$(CLIENT_SRC_DIR)/quick_match.c \
	$(CLIENT_SRC_DIR)/render_lod.c \
	$(CLIENT_SRC_DIR)/spectator_target.c \
	$(CLIENT_SRC_DIR)/results_summary.c \
	$(CLIENT_SRC_DIR)/results_transition.c \
	$(CLIENT_SRC_DIR)/voice.c \
	$(CLIENT_SRC_DIR)/voice_backend.c \
	$(CLIENT_SRC_DIR)/voice_codec.c \
	$(CLIENT_SRC_DIR)/voice_jitter.c \
	$(CLIENT_SRC_DIR)/voice_mixer.c \
	$(CLIENT_SRC_DIR)/voice_thread.c \
	$(CLIENT_SRC_DIR)/screen.c \
	$(CLIENT_SRC_DIR)/server_discovery.c \
	$(CLIENT_SRC_DIR)/server_discovery_state.c \
	$(CLIENT_SRC_DIR)/server_browser_model.c \
	$(CLIENT_SRC_DIR)/settings_session.c \
	$(CLIENT_SRC_DIR)/screens/screen_background.c \
	$(CLIENT_SRC_DIR)/screens/main_menu.c \
	$(CLIENT_SRC_DIR)/screens/game_mode_select.c \
	$(CLIENT_SRC_DIR)/screens/settings.c \
	$(CLIENT_SRC_DIR)/screens/help.c \
	$(CLIENT_SRC_DIR)/screens/credits.c \
	$(CLIENT_SRC_DIR)/screens/server_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_browser.c \
	$(CLIENT_SRC_DIR)/screens/lobby_roster.c \
	$(CLIENT_SRC_DIR)/screens/gameplay.c \
	$(CLIENT_SRC_DIR)/screens/results.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/intermission.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/net_telemetry.c \
	$(SHARED_SRC_DIR)/snapshot_replication.c \
	$(SHARED_SRC_DIR)/world_replication.c \
	$(SHARED_SRC_DIR)/connection.c

#Server source files
SERVER_SOURCES := \
	$(SERVER_SRC_DIR)/main.c \
	$(SERVER_SRC_DIR)/logger.c \
	$(SERVER_SRC_DIR)/account_auth.c \
	$(SERVER_SRC_DIR)/database.c \
	$(SERVER_SRC_DIR)/directory_registry.c \
	$(SERVER_SRC_DIR)/match_persistence.c \
	$(SERVER_SRC_DIR)/auth.c \
	$(SERVER_SRC_DIR)/input_admission.c \
	$(SERVER_SRC_DIR)/lobby_capacity.c \
	$(SERVER_SRC_DIR)/session_cleanup.c \
	$(SERVER_SRC_DIR)/snapshot_stats.c \
	$(SERVER_SRC_DIR)/rest_router.c \
	$(SERVER_SRC_DIR)/rest_account.c \
	$(SERVER_SRC_DIR)/rest_rate_limit.c \
	$(SERVER_SRC_DIR)/rest_server.c \
	$(SERVER_SRC_DIR)/voice_relay.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/intermission.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/net_telemetry.c \
	$(SHARED_SRC_DIR)/snapshot_replication.c \
	$(SHARED_SRC_DIR)/snapshot_scheduler.c \
	$(SHARED_SRC_DIR)/world_replication.c \
	$(SHARED_SRC_DIR)/connection.c

#Shared headers(dependencies for all modules)
SHARED_HEADERS := \
	$(SERVER_SRC_DIR)/account_auth.h \
	$(SHARED_SRC_DIR)/config.h \
	$(SHARED_SRC_DIR)/vec2.h \
	$(SHARED_SRC_DIR)/world.h \
	$(SHARED_SRC_DIR)/sim.h \
	$(SHARED_SRC_DIR)/intermission.h \
	$(SHARED_SRC_DIR)/protocol.h \
	$(SHARED_SRC_DIR)/profiler.h \
	$(SHARED_SRC_DIR)/net_telemetry.h \
	$(SHARED_SRC_DIR)/snapshot_replication.h \
	$(SHARED_SRC_DIR)/snapshot_scheduler.h \
	$(SHARED_SRC_DIR)/world_replication.h \
	$(SHARED_SRC_DIR)/lifecycle.h \
	$(SHARED_SRC_DIR)/connection.h \
	$(SHARED_SRC_DIR)/player_identity.h \
	$(CLIENT_SRC_DIR)/audio.h \
	$(CLIENT_SRC_DIR)/cursor.h \
	$(CLIENT_SRC_DIR)/input_scheduler.h \
	$(CLIENT_SRC_DIR)/layout.h \
	$(CLIENT_SRC_DIR)/match_feedback.h \
	$(CLIENT_SRC_DIR)/voice.h \
	$(CLIENT_SRC_DIR)/voice_backend.h \
	$(CLIENT_SRC_DIR)/voice_codec.h \
	$(CLIENT_SRC_DIR)/voice_jitter.h \
	$(CLIENT_SRC_DIR)/voice_mixer.h \
	$(CLIENT_SRC_DIR)/voice_thread.h \
	$(SERVER_SRC_DIR)/database.h \
	$(SERVER_SRC_DIR)/directory_registry.h \
	$(SERVER_SRC_DIR)/match_persistence.h \
	$(SERVER_SRC_DIR)/auth.h \
	$(SERVER_SRC_DIR)/input_admission.h \
	$(SERVER_SRC_DIR)/lobby_capacity.h \
	$(SERVER_SRC_DIR)/session_cleanup.h \
	$(SERVER_SRC_DIR)/snapshot_stats.h \
	$(SERVER_SRC_DIR)/rest_router.h \
	$(SERVER_SRC_DIR)/rest_account.h \
	$(SERVER_SRC_DIR)/rest_rate_limit.h \
	$(SERVER_SRC_DIR)/rest_server.h \
	$(SERVER_SRC_DIR)/voice_relay.h

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
	$(CLIENT_SRC_DIR)/account_flow.c \
	$(CLIENT_SRC_DIR)/audio.c \
	$(CLIENT_SRC_DIR)/chat_cache.c \
	$(CLIENT_SRC_DIR)/client_settings.c \
	$(CLIENT_SRC_DIR)/client_rest.c \
	$(CLIENT_SRC_DIR)/client_session_store.c \
	$(CLIENT_SRC_DIR)/cursor.c \
	$(CLIENT_SRC_DIR)/game.c \
	$(CLIENT_SRC_DIR)/game_mode_availability.c \
	$(CLIENT_SRC_DIR)/input_scheduler.c \
	$(CLIENT_SRC_DIR)/layout.c \
	$(CLIENT_SRC_DIR)/layout_metrics.c \
	$(CLIENT_SRC_DIR)/matchmaking_selector.c \
	$(CLIENT_SRC_DIR)/match_feedback.c \
	$(CLIENT_SRC_DIR)/match_presentation.c \
	$(CLIENT_SRC_DIR)/net.c \
	$(CLIENT_SRC_DIR)/prediction.c \
	$(CLIENT_SRC_DIR)/quick_match.c \
	$(CLIENT_SRC_DIR)/render_lod.c \
	$(CLIENT_SRC_DIR)/spectator_target.c \
	$(CLIENT_SRC_DIR)/results_summary.c \
	$(CLIENT_SRC_DIR)/results_transition.c \
	$(CLIENT_SRC_DIR)/voice.c \
	$(CLIENT_SRC_DIR)/voice_codec.c \
	$(CLIENT_SRC_DIR)/voice_jitter.c \
	$(CLIENT_SRC_DIR)/voice_mixer.c \
	$(CLIENT_SRC_DIR)/voice_thread.c \
	$(CLIENT_SRC_DIR)/screen.c \
	$(CLIENT_SRC_DIR)/server_discovery.c \
	$(CLIENT_SRC_DIR)/server_discovery_state.c \
	$(CLIENT_SRC_DIR)/server_browser_model.c \
	$(CLIENT_SRC_DIR)/settings_session.c \
	$(CLIENT_SRC_DIR)/screens/screen_background.c \
	$(CLIENT_SRC_DIR)/screens/main_menu.c \
	$(CLIENT_SRC_DIR)/screens/game_mode_select.c \
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
	$(SHARED_SRC_DIR)/net_telemetry.c \
	$(SHARED_SRC_DIR)/snapshot_replication.c \
	$(SHARED_SRC_DIR)/world_replication.c \
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
.PHONY: benchmark network-benchmark network-benchmark-test input-flood-test server-health-test rest-integration-test graceful-shutdown-integration-test udp-auth-integration-test

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
	@echo "  make network-benchmark Run real ENet loopback scenarios (1/64/256 clients)"
	@echo "  make server-health-test Validate the production UDP health probe"
	@echo "  make rest-integration-test Validate the HTTPS health endpoint"
	@echo "  make udp-auth-integration-test Validate registered identity collision handling"
	@echo "  make run-windows    Build and launch Windows client from WSL (bypasses WSLg audio/video)"
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

network-benchmark: $(NETWORK_BENCH_BIN)
	python3 scripts/network_benchmark.py --binary ./$(NETWORK_BENCH_BIN) \
		--clients "$(NETWORK_BENCH_CLIENTS)" --duration-ms $(NETWORK_BENCH_DURATION_MS) \
		--split-pieces $(NETWORK_BENCH_SPLIT_PIECES)
	@set +e; ./$(NETWORK_BENCH_BIN) --clients 1 --participants 1 --duration-ms 250 \
		--port 39888 --min-input-hz 1000000 >/dev/null 2>&1; status=$$?; set -e; \
	if [ $$status -ne 2 ]; then \
		echo "Error: expected threshold exit 2, got $$status"; exit 1; \
	fi; \
	echo "Deterministic threshold failure check passed."

network-benchmark-test: NETWORK_BENCH_DURATION_MS=500
network-benchmark-test: network-benchmark
	./$(NETWORK_BENCH_BIN) --clients 256 --participants 256 --split-pieces 1 \
		--duration-ms 500 --port 39889 --snapshot-rate 15 \
		--max-deadline-failures 5 >/dev/null
	./$(NETWORK_BENCH_BIN) --clients 256 --participants 256 --split-pieces 1 \
		--duration-ms 500 --port 39890 --snapshot-rate 20 --min-total-message-hz 12800 \
		--max-deadline-failures 5 >/dev/null
	@echo "Canonical 256-participant 15/20 Hz throughput gates passed."

server-health-test: $(SERVER_LINUX_BIN) $(SERVER_HEALTHCHECK_BIN)
	@set -eu; \
	port=$$((40000 + ($$$$ * 1103) % 20000)); tmp=/tmp/shroomio-health-$$$$; \
	mkdir -p "$$tmp"; server_pid=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then kill "$$server_pid" >/dev/null 2>&1 || true; wait "$$server_pid" >/dev/null 2>&1 || true; fi; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$port" --database "$$tmp/server.db" >"$$tmp/server.log" 2>&1 & server_pid=$$!; \
	ready=0; for attempt in $$(seq 1 200); do \
		if grep -q "server listening on 127.0.0.1:$$port/udp" "$$tmp/server.log"; then ready=1; break; fi; \
		if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
	done; \
	if [ $$ready -ne 1 ]; then echo "Health-test server failed to start"; cat "$$tmp/server.log"; exit 1; fi; \
	./$(SERVER_HEALTHCHECK_BIN) --host 127.0.0.1 --port "$$port" --timeout-ms 2000; \
	if ./$(SERVER_HEALTHCHECK_BIN) --host 127.0.0.1 --port $$((port + 1)) --timeout-ms 100 >/dev/null 2>&1; then \
		echo "Health check accepted an unavailable endpoint"; exit 1; \
	fi; \
	if ./$(SERVER_HEALTHCHECK_BIN) --port 0 >/dev/null 2>&1; then \
		echo "Health check accepted invalid arguments"; exit 1; \
	fi; \
	echo "Server health integration passed."

rest-integration-test: $(SERVER_LINUX_BIN) $(CLIENT_REST_SMOKE_BIN)
	@command -v openssl >/dev/null 2>&1 || { echo "openssl is required"; exit 1; }
	@command -v curl >/dev/null 2>&1 || { echo "curl is required"; exit 1; }
	@command -v python3 >/dev/null 2>&1 || { echo "python3 is required"; exit 1; }
	@set -eu; \
	port=$$((40000 + ($$$$ * 1103) % 20000)); udp_port=$$((port + 1)); \
	tmp=/tmp/shroomio-rest-$$$$; mkdir -p "$$tmp"; server_pid=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then kill "$$server_pid" >/dev/null 2>&1 || true; wait "$$server_pid" >/dev/null 2>&1 || true; fi; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 1 -subj "/CN=127.0.0.1" \
		-addext "subjectAltName=IP:127.0.0.1" \
		-keyout "$$tmp/key.pem" -out "$$tmp/cert.pem" >/dev/null 2>&1; \
	cat "$$tmp/key.pem" "$$tmp/cert.pem" >"$$tmp/rest.pem"; \
	./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$udp_port" --database "$$tmp/server.db" \
		--rest-bind 127.0.0.1 --rest-port "$$port" --rest-cert "$$tmp/rest.pem" \
		>"$$tmp/server.log" 2>&1 & server_pid=$$!; \
	ready=0; for attempt in $$(seq 1 200); do \
		if grep -q "REST HTTPS listener started" "$$tmp/server.log"; then ready=1; break; fi; \
		if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; sleep 0.02; \
	done; \
	if [ $$ready -ne 1 ]; then echo "REST integration server failed to start"; cat "$$tmp/server.log"; exit 1; fi; \
	./$(CLIENT_REST_SMOKE_BIN) "https://127.0.0.1:$$port" "$$tmp/cert.pem" \
		"$$tmp/client-session.cfg"; \
	response=$$(curl -ksS -D "$$tmp/headers" "https://127.0.0.1:$$port/health"); \
	test "$$response" = '{"status":"ok","service":"shroomio-server"}'; \
	grep -qi '^X-Request-ID: rest-' "$$tmp/headers"; \
	register_status=$$(curl -ksS -D "$$tmp/register.headers" -o "$$tmp/register.json" \
		-w '%{http_code}' -H 'Content-Type: application/json' \
		--data '{"username":"forest_cap","email":"Player@Example.COM","password":"correct horse battery"}' \
		"https://127.0.0.1:$$port/v1/account/register"); \
	test "$$register_status" = 201; grep -qi '^X-RateLimit-Limit: 5' "$$tmp/register.headers"; \
	python3 -c 'import json,sys; data=json.load(open(sys.argv[1])); assert data["account"]["email"] == "player@example.com"; assert data["session"]["expires_in"] == 900' "$$tmp/register.json"; \
	login_status=$$(curl -ksS -o "$$tmp/login.json" -w '%{http_code}' \
		-H 'Content-Type: application/json' \
		--data '{"identity":"PLAYER@EXAMPLE.COM","password":"correct horse battery"}' \
		"https://127.0.0.1:$$port/v1/account/login"); test "$$login_status" = 200; \
	access=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["access_token"])' "$$tmp/login.json"); \
	refresh=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["refresh_token"])' "$$tmp/login.json"); \
	me_status=$$(curl -ksS -o "$$tmp/me.json" -w '%{http_code}' -H "Authorization: Bearer $$access" \
		"https://127.0.0.1:$$port/v1/account/me"); test "$$me_status" = 200; \
	python3 -c 'import json,sys; assert json.load(open(sys.argv[1]))["username"] == "forest_cap"' "$$tmp/me.json"; \
	rotate_status=$$(curl -ksS -o "$$tmp/rotate.json" -w '%{http_code}' \
		-H 'Content-Type: application/json' --data "{\"refresh_token\":\"$$refresh\"}" \
		"https://127.0.0.1:$$port/v1/account/refresh"); test "$$rotate_status" = 200; \
	rotated_access=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["access_token"])' "$$tmp/rotate.json"); \
	reuse_status=$$(curl -ksS -o "$$tmp/reuse.json" -w '%{http_code}' \
		-H 'Content-Type: application/json' --data "{\"refresh_token\":\"$$refresh\"}" \
		"https://127.0.0.1:$$port/v1/account/refresh"); test "$$reuse_status" = 401; \
	revoked_status=$$(curl -ksS -o "$$tmp/revoked.json" -w '%{http_code}' \
		-H "Authorization: Bearer $$rotated_access" "https://127.0.0.1:$$port/v1/account/me"); \
	test "$$revoked_status" = 401; \
	login_status=$$(curl -ksS -o "$$tmp/login2.json" -w '%{http_code}' \
		-H 'Content-Type: application/json' \
		--data '{"identity":"forest_cap","password":"correct horse battery"}' \
		"https://127.0.0.1:$$port/v1/account/login"); test "$$login_status" = 200; \
	access=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["access_token"])' "$$tmp/login2.json"); \
	logout_status=$$(curl -ksS -o /dev/null -w '%{http_code}' -X POST \
		-H "Authorization: Bearer $$access" "https://127.0.0.1:$$port/v1/account/logout"); \
	test "$$logout_status" = 204; \
	logout_retry_status=$$(curl -ksS -o /dev/null -w '%{http_code}' -X POST \
		-H "Authorization: Bearer $$access" "https://127.0.0.1:$$port/v1/account/logout"); \
	test "$$logout_retry_status" = 204; \
	post_logout_status=$$(curl -ksS -o "$$tmp/post-logout.json" -w '%{http_code}' \
		-H "Authorization: Bearer $$access" "https://127.0.0.1:$$port/v1/account/me"); \
	test "$$post_logout_status" = 401; \
	for attempt in 1 2 3; do \
		bad_login_status=$$(curl -ksS -o /dev/null -w '%{http_code}' \
			-H 'Content-Type: application/json' \
			--data '{"identity":"forest_cap","password":"definitely wrong phrase"}' \
			"https://127.0.0.1:$$port/v1/account/login"); test "$$bad_login_status" = 401; \
	done; \
	limited_status=$$(curl -ksS -D "$$tmp/limited.headers" -o "$$tmp/limited.json" \
		-w '%{http_code}' -H 'Content-Type: application/json' \
		--data '{"identity":"forest_cap","password":"definitely wrong phrase"}' \
		"https://127.0.0.1:$$port/v1/account/login"); test "$$limited_status" = 429; \
	grep -qi '^Retry-After:' "$$tmp/limited.headers"; grep -q '"code":"rate_limited"' "$$tmp/limited.json"; \
	status=$$(curl -ksS -o "$$tmp/missing.json" -w '%{http_code}' -X POST \
		"https://127.0.0.1:$$port/health"); \
	test "$$status" = 404; grep -q '"code":"not_found"' "$$tmp/missing.json"; \
	grep -q 'rest_access method=GET path=/health status=200 .* body=redacted' "$$tmp/server.log"; \
	if grep -q 'correct horse battery\|Player@Example.COM\|player@example.com' "$$tmp/server.log"; then \
		echo "REST server log exposed account credentials"; exit 1; \
	fi; \
	if ./$(SERVER_LINUX_BIN) --smoke-test --rest-cert "$$tmp/missing.pem" \
		>"$$tmp/missing-cert.log" 2>&1; then echo "Server accepted a missing REST certificate"; exit 1; fi; \
	grep -q 'failed to start REST HTTPS listener' "$$tmp/missing-cert.log"; \
	echo "REST HTTPS integration passed."

input-flood-test: $(SERVER_LINUX_BIN) $(INPUT_FLOOD_CLIENT_BIN)
	@set -eu; \
	requested_port="$(INPUT_FLOOD_PORT)"; \
	db=/tmp/shroomio-input-flood-$$$$.db; \
	log=/tmp/shroomio-input-flood-$$$$.log; \
	server_pid=""; ready=0; attempt=0; seed=$$$$; port=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then \
			kill "$$server_pid" >/dev/null 2>&1 || true; \
			wait "$$server_pid" >/dev/null 2>&1 || true; \
		fi; \
		rm -f "$$db" "$$db-shm" "$$db-wal" "$$log"; \
	}; \
	trap cleanup EXIT INT TERM; \
	while [ $$attempt -lt 20 ]; do \
		if [ -n "$$requested_port" ]; then \
			port="$$requested_port"; \
		else \
			port=$$((40000 + (seed * 1103 + attempt * 7919) % 20000)); \
		fi; \
		rm -f "$$db" "$$db-shm" "$$db-wal"; \
		: >"$$log"; \
		./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$port" --database "$$db" >"$$log" 2>&1 & \
		server_pid=$$!; probe=0; \
		while [ $$probe -lt 200 ]; do \
			if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; \
			if grep -q "server listening on 127.0.0.1:$$port/udp" "$$log"; then ready=1; break; fi; \
			sleep 0.01; probe=$$((probe + 1)); \
		done; \
		if [ $$ready -eq 1 ]; then break; fi; \
		kill "$$server_pid" >/dev/null 2>&1 || true; \
		wait "$$server_pid" >/dev/null 2>&1 || true; server_pid=""; \
		if [ -n "$$requested_port" ]; then \
			echo "Error: input flood server failed to bind port $$port"; cat "$$log"; exit 1; \
		fi; \
		attempt=$$((attempt + 1)); \
	done; \
	if [ $$ready -ne 1 ] || ! kill -0 "$$server_pid" >/dev/null 2>&1; then \
		echo "Error: input flood server failed to bind an available port"; cat "$$log"; exit 1; \
	fi; \
	./$(INPUT_FLOOD_CLIENT_BIN) --port $$port

directory-integration-test: $(SERVER_LINUX_BIN) $(DIRECTORY_QUERY_CLIENT_BIN) $(SERVER_DISCOVERY_CLIENT_BIN) $(QUICK_MATCH_CLIENT_BIN)
	@set -eu; \
	base_port=$$((40000 + ($$$$ * 1103) % 20000)); directory_port=$$base_port; \
	game_one_port=$$((base_port + 1)); game_two_port=$$((base_port + 2)); \
	tmp=/tmp/shroomio-directory-$$$$; mkdir -p "$$tmp"; \
	directory_pid=""; game_one_pid=""; game_two_pid=""; \
	cleanup() { \
		for pid in "$$game_one_pid" "$$game_two_pid" "$$directory_pid"; do \
			if [ -n "$$pid" ]; then kill "$$pid" >/dev/null 2>&1 || true; wait "$$pid" >/dev/null 2>&1 || true; fi; \
		done; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	./$(SERVER_LINUX_BIN) --directory --bind 127.0.0.1 --directory-port $$directory_port >"$$tmp/directory.log" 2>&1 & directory_pid=$$!; \
	ready=0; for attempt in $$(seq 1 200); do \
		if grep -q "directory listening on 127.0.0.1:$$directory_port/udp" "$$tmp/directory.log"; then ready=1; break; fi; \
		if ! kill -0 "$$directory_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
	done; \
	if [ $$ready -ne 1 ]; then echo "Directory failed to start"; cat "$$tmp/directory.log"; exit 1; fi; \
	SHROOM_DIRECTORY_HOST=127.0.0.1 SHROOM_DIRECTORY_PORT=$$directory_port SHROOM_SERVER_NAME="Integration One" \
		./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port $$game_one_port --database "$$tmp/one.db" >"$$tmp/one.log" 2>&1 & game_one_pid=$$!; \
	SHROOM_DIRECTORY_HOST=127.0.0.1 SHROOM_DIRECTORY_PORT=$$directory_port SHROOM_SERVER_NAME="Integration Two" \
		./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port $$game_two_port --database "$$tmp/two.db" >"$$tmp/two.log" 2>&1 & game_two_pid=$$!; \
	registered=0; for attempt in $$(seq 1 700); do \
		if [ "$$(grep -c "directory heartbeat endpoint=" "$$tmp/directory.log" || true)" -ge 2 ]; then registered=1; break; fi; \
		if ! kill -0 "$$game_one_pid" >/dev/null 2>&1 || ! kill -0 "$$game_two_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
	done; \
	if [ $$registered -ne 1 ]; then echo "Game servers did not register"; cat "$$tmp/directory.log" "$$tmp/one.log" "$$tmp/two.log"; exit 1; fi; \
	./$(DIRECTORY_QUERY_CLIENT_BIN) 127.0.0.1 $$directory_port 2; \
	./$(SERVER_DISCOVERY_CLIENT_BIN) 127.0.0.1 $$directory_port 2; \
	./$(DIRECTORY_QUERY_CLIENT_BIN) 127.0.0.1 $$directory_port 2; \
	if grep -q "session activated:" "$$tmp/one.log" || grep -q "session activated:" "$$tmp/two.log"; then echo "Discovery traffic activated a player session"; exit 1; fi; \
	./$(QUICK_MATCH_CLIENT_BIN) 127.0.0.1 $$directory_port $$game_one_port $$game_two_port; \
	kill "$$game_one_pid"; wait "$$game_one_pid" >/dev/null 2>&1 || true; game_one_pid=""; \
	sleep 16; \
	./$(DIRECTORY_QUERY_CLIENT_BIN) 127.0.0.1 $$directory_port 1; \
	echo "Directory integration passed: two live servers registered and stopped server expired without player sessions."

snapshot-rate-integration-test: $(SERVER_LINUX_BIN) $(SNAPSHOT_RATE_PROBE_BIN)
	@set -eu; \
	tmp=/tmp/shroomio-snapshot-rate-$$$$; mkdir -p "$$tmp"; server_pid=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then kill "$$server_pid" >/dev/null 2>&1 || true; wait "$$server_pid" >/dev/null 2>&1 || true; fi; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	if ./$(SERVER_LINUX_BIN) --smoke-test --snapshot-rate 14 >"$$tmp/invalid-cli.log" 2>&1; then \
		echo "Server accepted invalid --snapshot-rate 14"; exit 1; \
	fi; \
	if SHROOM_SERVER_SNAPSHOT_RATE=21 ./$(SERVER_LINUX_BIN) --smoke-test >"$$tmp/invalid-env.log" 2>&1; then \
		echo "Server accepted invalid SHROOM_SERVER_SNAPSHOT_RATE=21"; exit 1; \
	fi; \
	for rate in 15 20; do \
		port=$$((40000 + ($$$$ * 1103 + rate * 7919) % 20000)); \
		db="$$tmp/rate-$$rate.db"; log="$$tmp/rate-$$rate.log"; \
		./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$port" --database "$$db" \
			--snapshot-rate "$$rate" >"$$log" 2>&1 & server_pid=$$!; \
		ready=0; for attempt in $$(seq 1 200); do \
			if grep -q "snapshot_rate=$$rate" "$$log"; then ready=1; break; fi; \
			if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
		done; \
		if [ $$ready -ne 1 ]; then echo "Snapshot-rate server failed to start"; cat "$$log"; exit 1; fi; \
		./$(SNAPSHOT_RATE_PROBE_BIN) "$$port" "$$rate"; \
		kill "$$server_pid"; wait "$$server_pid" >/dev/null 2>&1 || true; server_pid=""; \
	done

graceful-shutdown-integration-test: $(SERVER_LINUX_BIN) $(GRACEFUL_SHUTDOWN_CLIENT_BIN)
	@command -v python3 >/dev/null 2>&1 || { echo "python3 is required"; exit 1; }
	@set -eu; \
	port=$$((40000 + ($$$$ * 1103) % 20000)); tmp=/tmp/shroomio-shutdown-$$$$; \
	mkdir -p "$$tmp"; server_pid=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then kill "$$server_pid" >/dev/null 2>&1 || true; wait "$$server_pid" >/dev/null 2>&1 || true; fi; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$port" --database "$$tmp/server.db" \
		>"$$tmp/server.log" 2>&1 & server_pid=$$!; \
	ready=0; for attempt in $$(seq 1 200); do \
		if grep -q "server listening on 127.0.0.1:$$port/udp" "$$tmp/server.log"; then ready=1; break; fi; \
		if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
	done; \
	if [ $$ready -ne 1 ]; then echo "Graceful-shutdown server failed to start"; cat "$$tmp/server.log"; exit 1; fi; \
	./$(GRACEFUL_SHUTDOWN_CLIENT_BIN) "$$port"; \
	kill -TERM "$$server_pid"; wait "$$server_pid"; server_pid=""; \
	grep -q "persisted interrupted round .* participants=1" "$$tmp/server.log"; \
	./$(SERVER_LINUX_BIN) --smoke-test --bind 127.0.0.1 --port "$$port" \
		--database "$$tmp/server.db" >>"$$tmp/server.log" 2>&1; \
	python3 -c 'import sqlite3,sys; db=sqlite3.connect(sys.argv[1]); assert db.execute("SELECT count(*) FROM sessions WHERE status=\"aborted\"").fetchone()[0] == 1; assert db.execute("SELECT count(*) FROM session_participants").fetchone()[0] == 1; assert db.execute("SELECT count(*) FROM match_events WHERE event_type=\"match_interrupted\"").fetchone()[0] == 1; assert db.execute("SELECT count(*) FROM match_events WHERE event_type=\"participant_summary\"").fetchone()[0] == 1; assert db.execute("SELECT total_games_played FROM player_stats").fetchone()[0] == 1; assert db.execute("SELECT total_sessions FROM players").fetchone()[0] == 1' "$$tmp/server.db"; \
	echo "Graceful shutdown persistence integration passed."

udp-auth-integration-test: $(SERVER_LINUX_BIN) $(UDP_AUTH_COLLISION_CLIENT_BIN)
	@command -v python3 >/dev/null 2>&1 || { echo "python3 is required"; exit 1; }
	@set -eu; \
	port=$$((40000 + ($$$$ * 1103) % 20000)); tmp=/tmp/shroomio-udp-auth-$$$$; \
	mkdir -p "$$tmp"; server_pid=""; \
	cleanup() { \
		if [ -n "$$server_pid" ]; then kill "$$server_pid" >/dev/null 2>&1 || true; wait "$$server_pid" >/dev/null 2>&1 || true; fi; \
		rm -rf "$$tmp"; \
	}; \
	trap cleanup EXIT INT TERM; \
	./$(SERVER_LINUX_BIN) --bind 127.0.0.1 --port "$$port" --database "$$tmp/server.db" >"$$tmp/server.log" 2>&1 & server_pid=$$!; \
	ready=0; for attempt in $$(seq 1 200); do \
		if grep -q "server listening on 127.0.0.1:$$port/udp" "$$tmp/server.log"; then ready=1; break; fi; \
		if ! kill -0 "$$server_pid" >/dev/null 2>&1; then break; fi; sleep 0.01; \
	done; \
	if [ $$ready -ne 1 ]; then echo "UDP auth integration server failed to start"; cat "$$tmp/server.log"; exit 1; fi; \
	python3 -c 'import sqlite3,sys; db=sqlite3.connect(sys.argv[1]); cur=db.execute("INSERT INTO players (player_uuid, display_name) VALUES (?, ?)", ("registered-owner", "RegisteredOwner")); player_id=cur.lastrowid; db.execute("INSERT INTO player_stats (player_id) VALUES (?)", (player_id,)); db.execute("INSERT INTO users (player_id, username, password_hash, auth_method) VALUES (?, ?, ?, ?)", (player_id, "RegisteredOwner", "registered-only", "password")); db.commit(); db.close()' "$$tmp/server.db"; \
	./$(UDP_AUTH_COLLISION_CLIENT_BIN) "$$port" RegisteredOwner; \
	python3 -c 'import sqlite3,sys; db=sqlite3.connect(sys.argv[1]); rows=db.execute("SELECT auth_method FROM users WHERE username = ?", ("RegisteredOwner",)).fetchall(); assert rows == [("password",)], rows; assert db.execute("SELECT COUNT(*) FROM auth_tokens").fetchone()[0] == 0; db.close()' "$$tmp/server.db"; \
	echo "UDP registered identity collision integration passed."

run-windows:
	@command -v $(WINDOWS_CXX) >/dev/null 2>&1 || (printf '%s\n' 'Error: $(WINDOWS_CXX) not found. Install mingw-w64:' '  Ubuntu/Debian: sudo apt install mingw-w64' '  Fedora:       sudo dnf install mingw64-gcc-c++' '  Arch:         sudo pacman -S mingw-w64-gcc' && exit 1)
	@test -f $(VCPKG_WINDOWS_STAMP) || (printf '%s\n' 'Error: Windows vcpkg dependencies not installed.' 'Run: make vcpkg-install-windows' && exit 1)
	$(MAKE) $(CLIENT_WINDOWS_BIN)
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

$(NETWORK_BENCH_BIN): tools/network_benchmark.c $(SHARED_SRC_DIR)/net_telemetry.c $(SHARED_SRC_DIR)/snapshot_scheduler.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) tools/network_benchmark.c \
		$(SHARED_SRC_DIR)/net_telemetry.c $(SHARED_SRC_DIR)/snapshot_replication.c \
		$(SHARED_SRC_DIR)/snapshot_scheduler.c \
		-o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet -lm

$(SERVER_HEALTHCHECK_BIN): tools/server_healthcheck.c tools/server_healthcheck.h $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) -Itools $< -o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(CLIENT_REST_SMOKE_BIN): tools/client_rest_smoke.c $(CLIENT_SRC_DIR)/client_rest.c \
		$(CLIENT_SRC_DIR)/client_rest_curl.c $(CLIENT_SRC_DIR)/client_session_store.c | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $^ -o $@ -L$(VCPKG_LINUX_LIB_DIR) \
		-lcurl -lcjson -lssl -lcrypto -lz -ldl -lpthread -lm

$(INPUT_FLOOD_CLIENT_BIN): tools/input_flood_client.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $< $(SHARED_SRC_DIR)/snapshot_replication.c \
		-o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet -lm

$(DIRECTORY_QUERY_CLIENT_BIN): tools/directory_query_client.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $< -o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(SERVER_DISCOVERY_CLIENT_BIN): tools/server_discovery_client.c $(CLIENT_SRC_DIR)/server_discovery.c $(CLIENT_SRC_DIR)/server_discovery_state.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) tools/server_discovery_client.c \
		$(CLIENT_SRC_DIR)/server_discovery.c $(CLIENT_SRC_DIR)/server_discovery_state.c \
		-o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(QUICK_MATCH_CLIENT_BIN): tools/quick_match_client.c $(CLIENT_SRC_DIR)/server_discovery.c $(CLIENT_SRC_DIR)/server_discovery_state.c $(CLIENT_SRC_DIR)/quick_match.c $(CLIENT_SRC_DIR)/matchmaking_selector.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) tools/quick_match_client.c \
		$(CLIENT_SRC_DIR)/server_discovery.c $(CLIENT_SRC_DIR)/server_discovery_state.c \
		$(CLIENT_SRC_DIR)/quick_match.c $(CLIENT_SRC_DIR)/matchmaking_selector.c \
		-o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet -lm

$(SNAPSHOT_RATE_PROBE_BIN): tools/snapshot_rate_probe.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $< $(SHARED_SRC_DIR)/snapshot_replication.c \
		-o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet -lm

$(GRACEFUL_SHUTDOWN_CLIENT_BIN): tools/graceful_shutdown_client.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $< -o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(UDP_AUTH_COLLISION_CLIENT_BIN): tools/udp_auth_collision_client.c $(SHARED_HEADERS) | $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_SERVER_CFLAGS) $< -o $@ -L$(VCPKG_LINUX_LIB_DIR) -lenet

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
.PHONY: test unit-test imgui-test directory-integration-test snapshot-rate-integration-test valgrind-test valgrind-unit-test valgrind-server-smoke valgrind-imgui-test test-coverage test-clean

test: unit-test imgui-test input-flood-test directory-integration-test snapshot-rate-integration-test server-health-test rest-integration-test graceful-shutdown-integration-test udp-auth-integration-test

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
			test_lobby_capacity) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/lobby_capacity.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_auth) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/auth.c $(SERVER_SRC_DIR)/logger.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3 ;; \
			test_account_auth) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/account_auth.c $(SERVER_SRC_DIR)/database.c $(SERVER_SRC_DIR)/logger.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -largon2 -lcrypto -lsqlite3 ;; \
			test_lifecycle) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_server_healthcheck) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -Itools -DSHROOM_HEALTHCHECK_NO_MAIN \
					$$src $(UNITY_SRC) tools/server_healthcheck.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_screen) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_screen_background) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/screens/screen_background.c -o $$test_bin $(COVERAGE_LIBS) ;; \
test_connection) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_render_lod) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/render_lod.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_game_mode_availability) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/game_mode_availability.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_settings_session) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/settings_session.c $(CLIENT_SRC_DIR)/client_settings.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_client_settings_persistence) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/client_settings.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_client_rest) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/client_rest.c $(CLIENT_SRC_DIR)/client_session_store.c \
				-o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lcjson -lcrypto ;; \
		test_account_flow) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/account_flow.c $(CLIENT_SRC_DIR)/client_rest.c \
				$(CLIENT_SRC_DIR)/client_session_store.c -o $$test_bin $(COVERAGE_LIBS) \
				-L$(VCPKG_LINUX_LIB_DIR) -lcjson -lcrypto -lpthread ;; \
		test_server_browser_model) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/server_browser_model.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_server_discovery_state) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/server_discovery_state.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_matchmaking_selector) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/matchmaking_selector.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_quick_match) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/quick_match.c $(CLIENT_SRC_DIR)/matchmaking_selector.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_results_summary) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/results_summary.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_results_transition) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/results_transition.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_layout_metrics) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/layout_metrics.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_match_feedback) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/match_feedback.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_match_presentation) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/match_presentation.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_prediction) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/prediction.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_intermission) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/intermission.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_client_net) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/net.c $(CLIENT_SRC_DIR)/input_scheduler.c $(CLIENT_SRC_DIR)/chat_cache.c $(CLIENT_SRC_DIR)/results_transition.c $(SHARED_SRC_DIR)/net_telemetry.c $(SHARED_SRC_DIR)/snapshot_replication.c $(SHARED_SRC_DIR)/world_replication.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lenet ;; \
		test_snapshot_replication) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/snapshot_replication.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_input_scheduler) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/input_scheduler.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_input_admission) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/input_admission.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_directory_registry) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/directory_registry.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_rest_router) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/rest_router.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_rest_rate_limit) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/rest_rate_limit.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_spectator_target) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/spectator_target.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_chat_cache) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/chat_cache.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_net_telemetry) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/net_telemetry.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_snapshot_scheduler) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/snapshot_scheduler.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_world_replication) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/world_replication.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_sim) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_server_session_cleanup) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/session_cleanup.c $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_voice_relay) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/voice_relay.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_snapshot_stats) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/snapshot_stats.c $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
		test_match_persistence) \
			$(LINUX_CC) $(COVERAGE_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) \
				$$src $(UNITY_SRC) $(SERVER_SRC_DIR)/match_persistence.c $(SERVER_SRC_DIR)/database.c $(SERVER_SRC_DIR)/logger.c -o $$test_bin $(COVERAGE_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3 ;; \
			test_match_timer) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_idle_penalty) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_decay_zones) \
				$(LINUX_CC) $(COVERAGE_CFLAGS) \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c -o $$test_bin $(COVERAGE_LIBS) ;; \
			test_split_cost) \
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

$(TEST_BUILD_DIR)/test_server_healthcheck: $(UNIT_TESTS_DIR)/test_server_healthcheck.c $(UNITY_SRC) tools/server_healthcheck.c tools/server_healthcheck.h | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -Itools -DSHROOM_HEALTHCHECK_NO_MAIN \
		$(filter %.c,$^) -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_lobby_capacity: $(UNIT_TESTS_DIR)/test_lobby_capacity.c $(UNITY_SRC) $(SERVER_SRC_DIR)/lobby_capacity.c | $(UNITY_DIR)
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

$(TEST_BUILD_DIR)/test_voice_client: $(UNIT_TESTS_DIR)/test_voice_client.c $(UNITY_SRC) \
		$(CLIENT_SRC_DIR)/voice.c $(CLIENT_SRC_DIR)/voice_codec.c \
		$(CLIENT_SRC_DIR)/voice_jitter.c $(CLIENT_SRC_DIR)/voice_mixer.c \
		$(CLIENT_SRC_DIR)/voice_thread.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ \
		$(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lopus -lpthread

$(TEST_BUILD_DIR)/test_client_budget: $(UNIT_TESTS_DIR)/test_client_budget.c $(UNITY_SRC) | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_game_mode_availability: $(UNIT_TESTS_DIR)/test_game_mode_availability.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/game_mode_availability.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_settings_session: $(UNIT_TESTS_DIR)/test_settings_session.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/settings_session.c $(CLIENT_SRC_DIR)/client_settings.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_client_settings_persistence: $(UNIT_TESTS_DIR)/test_client_settings_persistence.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/client_settings.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_client_rest: $(UNIT_TESTS_DIR)/test_client_rest.c $(UNITY_SRC) \
		$(CLIENT_SRC_DIR)/client_rest.c $(CLIENT_SRC_DIR)/client_session_store.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ \
		$(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lcjson -lcrypto

$(TEST_BUILD_DIR)/test_account_flow: $(UNIT_TESTS_DIR)/test_account_flow.c $(UNITY_SRC) \
		$(CLIENT_SRC_DIR)/account_flow.c $(CLIENT_SRC_DIR)/client_rest.c \
		$(CLIENT_SRC_DIR)/client_session_store.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ \
		$(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lcjson -lcrypto -lpthread

$(TEST_BUILD_DIR)/test_server_browser_model: $(UNIT_TESTS_DIR)/test_server_browser_model.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/server_browser_model.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_server_discovery_state: $(UNIT_TESTS_DIR)/test_server_discovery_state.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/server_discovery_state.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_matchmaking_selector: $(UNIT_TESTS_DIR)/test_matchmaking_selector.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/matchmaking_selector.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_quick_match: $(UNIT_TESTS_DIR)/test_quick_match.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/quick_match.c $(CLIENT_SRC_DIR)/matchmaking_selector.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_results_summary: $(UNIT_TESTS_DIR)/test_results_summary.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/results_summary.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_results_transition: $(UNIT_TESTS_DIR)/test_results_transition.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/results_transition.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_spectator_target: $(UNIT_TESTS_DIR)/test_spectator_target.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/spectator_target.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_layout_metrics: $(UNIT_TESTS_DIR)/test_layout_metrics.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/layout_metrics.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_match_feedback: $(UNIT_TESTS_DIR)/test_match_feedback.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/match_feedback.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_match_presentation: $(UNIT_TESTS_DIR)/test_match_presentation.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/match_presentation.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_prediction: $(UNIT_TESTS_DIR)/test_prediction.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/prediction.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_client_net: $(UNIT_TESTS_DIR)/test_client_net.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/net.c $(CLIENT_SRC_DIR)/input_scheduler.c $(CLIENT_SRC_DIR)/chat_cache.c $(CLIENT_SRC_DIR)/results_transition.c $(SHARED_SRC_DIR)/net_telemetry.c $(SHARED_SRC_DIR)/snapshot_replication.c $(SHARED_SRC_DIR)/world_replication.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lenet

$(TEST_BUILD_DIR)/test_snapshot_replication: $(UNIT_TESTS_DIR)/test_snapshot_replication.c $(UNITY_SRC) $(SHARED_SRC_DIR)/snapshot_replication.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_input_scheduler: $(UNIT_TESTS_DIR)/test_input_scheduler.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/input_scheduler.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_input_admission: $(UNIT_TESTS_DIR)/test_input_admission.c $(UNITY_SRC) $(SERVER_SRC_DIR)/input_admission.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_directory_registry: $(UNIT_TESTS_DIR)/test_directory_registry.c $(UNITY_SRC) $(SERVER_SRC_DIR)/directory_registry.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_rest_router: $(UNIT_TESTS_DIR)/test_rest_router.c $(UNITY_SRC) $(SERVER_SRC_DIR)/rest_router.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_rest_rate_limit: $(UNIT_TESTS_DIR)/test_rest_rate_limit.c $(UNITY_SRC) $(SERVER_SRC_DIR)/rest_rate_limit.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_chat_cache: $(UNIT_TESTS_DIR)/test_chat_cache.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/chat_cache.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_net_telemetry: $(UNIT_TESTS_DIR)/test_net_telemetry.c $(UNITY_SRC) $(SHARED_SRC_DIR)/net_telemetry.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_snapshot_scheduler: $(UNIT_TESTS_DIR)/test_snapshot_scheduler.c $(UNITY_SRC) $(SHARED_SRC_DIR)/snapshot_scheduler.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_world_replication: $(UNIT_TESTS_DIR)/test_world_replication.c $(UNITY_SRC) $(SHARED_SRC_DIR)/world_replication.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_sim: $(UNIT_TESTS_DIR)/test_sim.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_server_session_cleanup: $(UNIT_TESTS_DIR)/test_server_session_cleanup.c $(UNITY_SRC) $(SERVER_SRC_DIR)/session_cleanup.c $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_voice_relay: $(UNIT_TESTS_DIR)/test_voice_relay.c $(UNITY_SRC) $(SERVER_SRC_DIR)/voice_relay.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_snapshot_stats: $(UNIT_TESTS_DIR)/test_snapshot_stats.c $(UNITY_SRC) $(SERVER_SRC_DIR)/snapshot_stats.c $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_match_persistence: $(UNIT_TESTS_DIR)/test_match_persistence.c $(UNITY_SRC) $(SERVER_SRC_DIR)/match_persistence.c $(SERVER_SRC_DIR)/database.c $(SERVER_SRC_DIR)/logger.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3

$(TEST_BUILD_DIR)/test_match_timer: $(UNIT_TESTS_DIR)/test_match_timer.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_intermission: $(UNIT_TESTS_DIR)/test_intermission.c $(UNITY_SRC) $(SHARED_SRC_DIR)/intermission.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_idle_penalty: $(UNIT_TESTS_DIR)/test_idle_penalty.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_decay_zones: $(UNIT_TESTS_DIR)/test_decay_zones.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_auth: $(UNIT_TESTS_DIR)/test_auth.c $(UNITY_SRC) $(SERVER_SRC_DIR)/auth.c $(SERVER_SRC_DIR)/logger.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -lsqlite3

$(TEST_BUILD_DIR)/test_account_auth: $(UNIT_TESTS_DIR)/test_account_auth.c $(UNITY_SRC) $(SERVER_SRC_DIR)/account_auth.c $(SERVER_SRC_DIR)/database.c $(SERVER_SRC_DIR)/logger.c | $(UNITY_DIR) $(VCPKG_LINUX_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(SERVER_SRC_DIR) -I$(VCPKG_LINUX_INCLUDE_DIR) $^ -o $@ $(TEST_LIBS) -L$(VCPKG_LINUX_LIB_DIR) -largon2 -lcrypto -lsqlite3

$(TEST_BUILD_DIR)/test_render_lod: $(UNIT_TESTS_DIR)/test_render_lod.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/render_lod.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(CLIENT_SRC_DIR) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_split_cost: $(UNIT_TESTS_DIR)/test_split_cost.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_eject: $(UNIT_TESTS_DIR)/test_eject.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_powerups: $(UNIT_TESTS_DIR)/test_powerups.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_split_steering: $(UNIT_TESTS_DIR)/test_split_steering.c $(UNITY_SRC) $(SHARED_SRC_DIR)/sim.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

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
	cppcheck --enable=warning,style,performance,portability --inline-suppr \
		--error-exitcode=1 \
		--include=src/shared/config.h \
		src/

# Run all linters
lint: format-check cppcheck
	@echo "All lint checks passed."

# Run full CI validation locally
check: lint test network-benchmark-test
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
