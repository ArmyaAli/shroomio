# =============================================================================
# shroomio Makefile
# =============================================================================
# Build system for the shroomio multiplayer arena game.
#
# Sections:
#   1. Project Configuration - Core project settings and version info
#   2. Directory Paths       - Source, build, and output directories
#   3. Compiler Settings     - Compilers, flags, and libraries
#   4. Source Files          - Lists of source files and objects
#   5. Build Targets         - Main build targets (linux, windows, server)
#   6. Compilation Rules     - Pattern rules for compiling objects
#   7. Vendor Dependencies   - raylib, raygui, enet, Unity downloads
#   8. Test Targets          - Unit test compilation and execution
#   9. Docker Targets        - Container builds for server and devcontainer
#   10. Lint & Format        - Code formatting and static analysis
#   11. Documentation        - LaTeX specification build
#   12. Cleanup              - Clean and distclean targets
# =============================================================================

# =============================================================================
# 1. Project Configuration
# =============================================================================
PROJECT := shroomio

RAYLIB_VERSION := 5.5
RAYGUI_VERSION := 4.0
ENET_VERSION   := 1.3.18
UNITY_VERSION  := 2.6.0

# =============================================================================
# 2. Directory Paths
# =============================================================================
SRC_DIR         := src
CLIENT_SRC_DIR  := $(SRC_DIR)/client
SERVER_SRC_DIR  := $(SRC_DIR)/server
SHARED_SRC_DIR  := $(SRC_DIR)/shared
BUILD_DIR       := build
DIST_DIR        := dist
TESTS_DIR       := tests
UNIT_TESTS_DIR  := $(TESTS_DIR)/unit

LINUX_BUILD_DIR   := $(BUILD_DIR)/linux
WINDOWS_BUILD_DIR := $(BUILD_DIR)/windows
TEST_BUILD_DIR    := $(BUILD_DIR)/tests

# Output binaries
LINUX_BIN  := $(DIST_DIR)/$(PROJECT)
WINDOWS_BIN := $(DIST_DIR)/$(PROJECT).exe
SERVER_BIN := $(DIST_DIR)/$(PROJECT)-server

# =============================================================================
# 3. Vendor Dependencies
# =============================================================================
RAYLIB_DIR := vendor/raylib-$(RAYLIB_VERSION)
RAYLIB_URL := https://github.com/raysan5/raylib/archive/refs/tags/$(RAYLIB_VERSION).tar.gz
RAYLIB_SRC_DIR := $(RAYLIB_DIR)/src
RAYLIB_GLFW_INCLUDE_DIR := $(RAYLIB_SRC_DIR)/external/glfw/include

RAYGUI_DIR := vendor/raygui-$(RAYGUI_VERSION)
RAYGUI_URL := https://github.com/raysan5/raygui/archive/refs/tags/$(RAYGUI_VERSION).tar.gz
RAYGUI_SRC_DIR := $(RAYGUI_DIR)/src

ENET_DIR := vendor/enet-$(ENET_VERSION)
ENET_URL := https://github.com/lsalzman/enet/archive/refs/tags/v$(ENET_VERSION).tar.gz
ENET_INCLUDE_DIR := $(ENET_DIR)/include

UNITY_DIR := vendor/Unity-$(UNITY_VERSION)
UNITY_URL := https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v$(UNITY_VERSION).tar.gz
UNITY_SRC_DIR := $(UNITY_DIR)/src
UNITY_SRC := $(UNITY_SRC_DIR)/unity.c
UNITY_INCLUDE := -I$(UNITY_SRC_DIR)

# Warning flags
COMMON_WARNINGS := -Wall -Wextra -Wpedantic
VENDOR_WARNINGS := -Wall -Wextra

# Include directories
COMMON_INCLUDE_DIRS := -I$(SRC_DIR) -I$(CLIENT_SRC_DIR) -I$(SERVER_SRC_DIR) -I$(SHARED_SRC_DIR)

# Test compiler flags
TEST_CFLAGS := -std=c11 -O0 -g $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
               $(UNITY_INCLUDE) -DTEST_MODE -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
TEST_LIBS   := -lm

# =============================================================================
# 4. Compiler Settings
# =============================================================================
LINUX_CC   ?= cc
WINDOWS_CC ?= x86_64-w64-mingw32-gcc
AR         ?= ar

# Tools
CURL    ?= curl
TAR     ?= tar
MKDIR_P ?= mkdir -p
RM_RF   ?= rm -rf
DOCKER  ?= docker

# Client compiler flags (raylib-based)
COMMON_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
                 -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) -I$(RAYGUI_SRC_DIR) \
                 -I$(ENET_INCLUDE_DIR)
LINUX_CFLAGS   := $(COMMON_CFLAGS) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
WINDOWS_CFLAGS := $(COMMON_CFLAGS) -DPLATFORM_DESKTOP

# Server compiler flags (headless, ENet-based)
SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) \
                 -I$(ENET_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
SERVER_LIBS   := -lm -lsqlite3

# Raylib vendor compiler flags
LINUX_RAYLIB_CFLAGS   := -std=c11 -O2 $(VENDOR_WARNINGS) -I$(SRC_DIR) \
                         -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) \
                         -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE -D_GLFW_X11
WINDOWS_RAYLIB_CFLAGS := -std=c11 -O2 $(VENDOR_WARNINGS) -I$(SRC_DIR) \
                         -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) \
                         -DPLATFORM_DESKTOP

# Test compiler flags (UNITY_INCLUDE defined in vendor section)
TEST_LIBS := -lm

# Platform link libraries
LINUX_LIBS   := -lGL -lm -ldl -lpthread -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lasound
WINDOWS_LIBS := -lopengl32 -lgdi32 -lwinmm -lws2_32

# =============================================================================
# 5. Source Files and Objects
# =============================================================================

# Raylib source files
RAYLIB_SOURCE_NAMES := rcore rmodels rshapes rtext rtextures utils raudio rglfw
LINUX_RAYLIB_OBJECTS   := $(addprefix $(LINUX_BUILD_DIR)/raylib/,$(addsuffix .o,$(RAYLIB_SOURCE_NAMES)))
WINDOWS_RAYLIB_OBJECTS := $(addprefix $(WINDOWS_BUILD_DIR)/raylib/,$(addsuffix .o,$(RAYLIB_SOURCE_NAMES)))
LINUX_RAYLIB_LIB   := $(LINUX_BUILD_DIR)/libraylib.a
WINDOWS_RAYLIB_LIB := $(WINDOWS_BUILD_DIR)/libraylib.a

# Client source files
CLIENT_SOURCES := \
	$(CLIENT_SRC_DIR)/main.c \
	$(CLIENT_SRC_DIR)/client_settings.c \
	$(CLIENT_SRC_DIR)/game.c \
	$(CLIENT_SRC_DIR)/net.c \
	$(CLIENT_SRC_DIR)/screen.c \
	$(CLIENT_SRC_DIR)/raygui_impl.c \
	$(CLIENT_SRC_DIR)/screens/main_menu.c \
	$(CLIENT_SRC_DIR)/screens/settings.c \
	$(CLIENT_SRC_DIR)/screens/help.c \
	$(CLIENT_SRC_DIR)/screens/credits.c \
	$(CLIENT_SRC_DIR)/screens/server_browser.c \
	$(CLIENT_SRC_DIR)/screens/gameplay.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/connection.c

# Server source files
SERVER_SOURCES := \
	$(SERVER_SRC_DIR)/main.c \
	$(SERVER_SRC_DIR)/logger.c \
	$(SERVER_SRC_DIR)/auth.c \
	$(SHARED_SRC_DIR)/sim.c \
	$(SHARED_SRC_DIR)/lifecycle.c \
	$(SHARED_SRC_DIR)/connection.c

# Shared headers (dependencies for all modules)
SHARED_HEADERS := \
	$(SHARED_SRC_DIR)/config.h \
	$(SHARED_SRC_DIR)/vec2.h \
	$(SHARED_SRC_DIR)/world.h \
	$(SHARED_SRC_DIR)/sim.h \
	$(SHARED_SRC_DIR)/protocol.h \
	$(SHARED_SRC_DIR)/lifecycle.h \
	$(SHARED_SRC_DIR)/connection.h \
	$(SERVER_SRC_DIR)/auth.h

# ENet source files
ENET_COMMON_SOURCE_NAMES  := callbacks compress host list packet peer protocol
ENET_LINUX_SOURCE_NAMES   := unix
ENET_WINDOWS_SOURCE_NAMES := win32
LINUX_ENET_SOURCES   := $(addprefix $(ENET_DIR)/,$(addsuffix .c,$(ENET_COMMON_SOURCE_NAMES) $(ENET_LINUX_SOURCE_NAMES)))
WINDOWS_ENET_SOURCES := $(addprefix $(ENET_DIR)/,$(addsuffix .c,$(ENET_COMMON_SOURCE_NAMES) $(ENET_WINDOWS_SOURCE_NAMES)))

# Object files
LINUX_APP_OBJECTS   := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
WINDOWS_APP_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(WINDOWS_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
SERVER_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(SERVER_SOURCES))
LINUX_ENET_OBJECTS  := $(patsubst $(ENET_DIR)/%.c,$(LINUX_BUILD_DIR)/vendor/enet/%.o,$(LINUX_ENET_SOURCES))
WINDOWS_ENET_OBJECTS := $(patsubst $(ENET_DIR)/%.c,$(WINDOWS_BUILD_DIR)/vendor/enet/%.o,$(WINDOWS_ENET_SOURCES))
SERVER_ENET_OBJECTS := $(LINUX_ENET_OBJECTS)

# Test files
TEST_SRCS := $(wildcard $(UNIT_TESTS_DIR)/*.c)
TEST_BINS := $(patsubst $(UNIT_TESTS_DIR)/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))

# =============================================================================
# 5. Build Targets
# =============================================================================
.PHONY: all linux windows server run run-server run-windows help

all: linux

help:
	@echo ""
	@echo "shroomio build system"
	@echo "====================="
	@echo ""
	@echo "Build targets:"
	@echo "  make linux          Build the Linux client binary"
	@echo "  make windows        Build the Windows client binary (requires mingw-w64)"
	@echo "  make server         Build the Linux headless server binary"
	@echo "  make run            Build and run the Linux client"
	@echo "  make run-server     Build and run the Linux server"
	@echo "  make run-windows    Build and run the Windows client (via WSL)"
	@echo ""
	@echo "Quality targets:"
	@echo "  make test           Run all unit tests"
	@echo "  make test-coverage  Run tests with coverage report"
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
	@echo "  make devcontainer-down          Stop the dev container"
	@echo "  make devcontainer-opencode-sync Refresh OpenCode state"
	@echo "  make devcontainer-git-identity  Copy host git identity"
	@echo "  make devcontainer-github-token  Store GitHub token"
	@echo "  make devcontainer-github-status Check GitHub auth"
	@echo ""
	@echo "Documentation:"
	@echo "  make spec           Build the LaTeX specification PDF"
	@echo ""
	@echo "Vendor dependencies:"
	@echo "  make vendor         Download all vendor dependencies"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean          Remove build and dist artifacts"
	@echo "  make distclean      Remove build artifacts and vendor sources"
	@echo "  make test-clean     Remove test build artifacts"
	@echo ""

linux: $(LINUX_BIN)

windows: $(WINDOWS_BIN)

server: $(SERVER_BIN)

run: $(LINUX_BIN)
	./$(LINUX_BIN)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-windows: $(WINDOWS_BIN)
	./$(WINDOWS_BIN)

# =============================================================================
# 6. Compilation Rules
# =============================================================================

# Link targets
$(LINUX_BIN): $(LINUX_APP_OBJECTS) $(LINUX_RAYLIB_LIB) $(LINUX_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(LINUX_CC) $(LINUX_APP_OBJECTS) $(LINUX_RAYLIB_LIB) $(LINUX_ENET_OBJECTS) -o $@ $(LINUX_LIBS)

$(WINDOWS_BIN): $(WINDOWS_APP_OBJECTS) $(WINDOWS_RAYLIB_LIB) $(WINDOWS_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(WINDOWS_CC) -static $(WINDOWS_APP_OBJECTS) $(WINDOWS_RAYLIB_LIB) $(WINDOWS_ENET_OBJECTS) -o $@ $(WINDOWS_LIBS)

$(SERVER_BIN): $(SERVER_OBJECTS) $(SERVER_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(LINUX_CC) $(SERVER_OBJECTS) $(SERVER_ENET_OBJECTS) -o $@ $(SERVER_LIBS)

# Static libraries
$(LINUX_RAYLIB_LIB): $(LINUX_RAYLIB_OBJECTS)
	$(AR) rcs $@ $(LINUX_RAYLIB_OBJECTS)

$(WINDOWS_RAYLIB_LIB): $(WINDOWS_RAYLIB_OBJECTS)
	$(AR) rcs $@ $(WINDOWS_RAYLIB_OBJECTS)

# Raylib object compilation
$(LINUX_BUILD_DIR)/raylib/%.o: $(RAYLIB_SRC_DIR)/%.c $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_RAYLIB_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/raylib/%.o: $(RAYLIB_SRC_DIR)/%.c $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_RAYLIB_CFLAGS) -c $< -o $@

# Client object compilation
$(LINUX_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_CFLAGS) -I$(ENET_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE -c $< -o $@

$(WINDOWS_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_CFLAGS) -I$(ENET_INCLUDE_DIR) -c $< -o $@

# Shared object compilation
$(LINUX_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(SERVER_CFLAGS) -c $< -o $@

# Server object compilation
$(LINUX_BUILD_DIR)/server/%.o: $(SERVER_SRC_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

# ENet object compilation
$(LINUX_BUILD_DIR)/vendor/enet/%.o: $(ENET_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/vendor/enet/%.o: $(ENET_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) -I$(ENET_INCLUDE_DIR) -c $< -o $@

# =============================================================================
# 7. Vendor Dependencies
# =============================================================================
.PHONY: vendor

vendor: $(RAYLIB_DIR) $(RAYGUI_DIR) $(ENET_DIR) $(UNITY_DIR)

$(ENET_DIR):
	$(MKDIR_P) vendor build
	$(CURL) -L $(ENET_URL) -o build/enet.tar.gz
	$(TAR) -xzf build/enet.tar.gz -C vendor

$(RAYLIB_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(RAYLIB_URL) -o $(BUILD_DIR)/raylib.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/raylib.tar.gz -C vendor

$(RAYGUI_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(RAYGUI_URL) -o $(BUILD_DIR)/raygui.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/raygui.tar.gz -C vendor

$(UNITY_DIR):
	@$(MKDIR_P) vendor $(BUILD_DIR)
	$(CURL) -L $(UNITY_URL) -o $(BUILD_DIR)/unity.tar.gz
	$(TAR) -xzf $(BUILD_DIR)/unity.tar.gz -C vendor

# =============================================================================
# 8. Test Targets
# =============================================================================
.PHONY: test test-coverage test-clean

test: $(TEST_BINS)
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

# Test with coverage (requires gcovr)
test-coverage:
	@echo "Building tests with coverage instrumentation..."
	@$(MKDIR_P) $(TEST_BUILD_DIR)
	@for src in $(TEST_SRCS); do \
		test_name=$$(basename $$src .c); \
		echo "Compiling $$test_name..."; \
	done
	@failed=0; total=0; \
	for src in $(TEST_SRCS); do \
		test_name=$$(basename $$src .c); \
		test_bin=$(TEST_BUILD_DIR)/$$test_name; \
		total=$$((total + 1)); \
		case $$test_name in \
			test_lifecycle) \
				$(LINUX_CC) $(TEST_CFLAGS) -fprofile-arcs -ftest-coverage \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c -o $$test_bin $(TEST_LIBS) -lgcov ;; \
			test_screen) \
				$(LINUX_CC) $(TEST_CFLAGS) -fprofile-arcs -ftest-coverage \
					$$src $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c -o $$test_bin $(TEST_LIBS) -lgcov ;; \
			test_connection) \
				$(LINUX_CC) $(TEST_CFLAGS) -fprofile-arcs -ftest-coverage \
					$$src $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c -o $$test_bin $(TEST_LIBS) -lgcov ;; \
			*) \
				$(LINUX_CC) $(TEST_CFLAGS) -fprofile-arcs -ftest-coverage \
					$$src $(UNITY_SRC) -o $$test_bin $(TEST_LIBS) -lgcov ;; \
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
	@gcovr -r . --html --html-details -o coverage.html
	@echo "Coverage report generated: coverage.html"

test-clean:
	$(RM_RF) $(TEST_BUILD_DIR) coverage.html *.gcda *.gcno

# Specific test targets with explicit dependencies
$(TEST_BUILD_DIR)/test_lifecycle: $(UNIT_TESTS_DIR)/test_lifecycle.c $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_screen: $(UNIT_TESTS_DIR)/test_screen.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_connection: $(UNIT_TESTS_DIR)/test_connection.c $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_auth: $(UNIT_TESTS_DIR)/test_auth.c $(UNITY_SRC) $(SERVER_SRC_DIR)/auth.c $(SERVER_SRC_DIR)/logger.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) -I$(SERVER_SRC_DIR) $^ -o $@ $(TEST_LIBS) -lsqlite3

$(TEST_BUILD_DIR)/%: $(UNIT_TESTS_DIR)/%.c $(UNITY_SRC) | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

# =============================================================================
# 9. Docker Targets
# =============================================================================
.PHONY: docker-server docker-run-server docker-logs
.PHONY: devcontainer-build devcontainer-up devcontainer-shell devcontainer-down
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
docker-server:
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
	$(RM_RF) vendor
