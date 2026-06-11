PROJECT := shroomio
RAYLIB_VERSION := 5.5
RAYLIB_DIR := vendor/raylib-$(RAYLIB_VERSION)
RAYLIB_URL := https://github.com/raysan5/raylib/archive/refs/tags/$(RAYLIB_VERSION).tar.gz
RAYLIB_SRC_DIR := $(RAYLIB_DIR)/src
RAYLIB_GLFW_INCLUDE_DIR := $(RAYLIB_SRC_DIR)/external/glfw/include
RAYGUI_VERSION := 4.0
RAYGUI_DIR := vendor/raygui-$(RAYGUI_VERSION)
RAYGUI_URL := https://github.com/raysan5/raygui/archive/refs/tags/$(RAYGUI_VERSION).tar.gz
RAYGUI_SRC_DIR := $(RAYGUI_DIR)/src
ENET_DIR := vendor/enet
ENET_INCLUDE_DIR := $(ENET_DIR)/include
UNITY_VERSION := 2.6.0
UNITY_DIR := vendor/Unity-$(UNITY_VERSION)
UNITY_URL := https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v$(UNITY_VERSION).tar.gz
UNITY_SRC_DIR := $(UNITY_DIR)/src
SRC_DIR := src
CLIENT_SRC_DIR := $(SRC_DIR)/client
SERVER_SRC_DIR := $(SRC_DIR)/server
SHARED_SRC_DIR := $(SRC_DIR)/shared
BUILD_DIR := build
DIST_DIR := dist
TESTS_DIR := tests
UNIT_TESTS_DIR := $(TESTS_DIR)/unit
UNITY_SRC := $(UNITY_SRC_DIR)/unity.c
UNITY_INCLUDE := -I$(UNITY_SRC_DIR)

LINUX_BUILD_DIR := $(BUILD_DIR)/linux
WINDOWS_BUILD_DIR := $(BUILD_DIR)/windows

LINUX_BIN := $(DIST_DIR)/$(PROJECT)
WINDOWS_BIN := $(DIST_DIR)/$(PROJECT).exe
SERVER_BIN := $(DIST_DIR)/$(PROJECT)-server

LINUX_CC ?= cc
WINDOWS_CC ?= x86_64-w64-mingw32-gcc
AR ?= ar
CURL ?= curl
TAR ?= tar
MKDIR_P ?= mkdir -p
RM_RF ?= rm -rf
DOCKER ?= docker
SERVER_IMAGE ?= shroomio-server:dev
SERVER_CONTAINER ?= shroomio-server-1
DEVCONTAINER ?= devcontainer
DEVCONTAINER_IMAGE ?= shroomio-devcontainer:dev
DEVCONTAINER_CONTAINER ?= shroomio-devcontainer
GIT_USER_NAME := $(shell git config --global --get user.name)
GIT_USER_EMAIL := $(shell git config --global --get user.email)
DEVCONTAINER_SECRET_DIR ?= $(HOME)/.config/shroomio-devcontainer
SPEC_SRC := design/shroomio-specification.tex
SPEC_OUT := dist/latex
DEVCONTAINER_GITHUB_TOKEN_FILE ?= $(DEVCONTAINER_SECRET_DIR)/github-token

COMMON_WARNINGS := -Wall -Wextra -Wpedantic
COMMON_INCLUDE_DIRS := -I$(SRC_DIR) -I$(CLIENT_SRC_DIR) -I$(SERVER_SRC_DIR) -I$(SHARED_SRC_DIR)
COMMON_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) -I$(RAYGUI_SRC_DIR)
SERVER_CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) -I$(ENET_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE
SERVER_LIBS := -lm -lsqlite3

VENDOR_WARNINGS := -Wall -Wextra

LINUX_CFLAGS := $(COMMON_CFLAGS) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
WINDOWS_CFLAGS := $(COMMON_CFLAGS) -DPLATFORM_DESKTOP

LINUX_RAYLIB_CFLAGS := -std=c11 -O2 $(VENDOR_WARNINGS) -I$(SRC_DIR) -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE -D_GLFW_X11
WINDOWS_RAYLIB_CFLAGS := -std=c11 -O2 $(VENDOR_WARNINGS) -I$(SRC_DIR) -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_GLFW_INCLUDE_DIR) -DPLATFORM_DESKTOP

RAYLIB_SOURCE_NAMES := rcore rmodels rshapes rtext rtextures utils raudio rglfw

LINUX_RAYLIB_OBJECTS := $(addprefix $(LINUX_BUILD_DIR)/raylib/,$(addsuffix .o,$(RAYLIB_SOURCE_NAMES)))
WINDOWS_RAYLIB_OBJECTS := $(addprefix $(WINDOWS_BUILD_DIR)/raylib/,$(addsuffix .o,$(RAYLIB_SOURCE_NAMES)))

CLIENT_SOURCES := $(CLIENT_SRC_DIR)/main.c $(CLIENT_SRC_DIR)/game.c $(CLIENT_SRC_DIR)/net.c $(CLIENT_SRC_DIR)/screen.c $(CLIENT_SRC_DIR)/raygui_impl.c $(CLIENT_SRC_DIR)/screens/main_menu.c $(CLIENT_SRC_DIR)/screens/settings.c $(CLIENT_SRC_DIR)/screens/help.c $(CLIENT_SRC_DIR)/screens/credits.c $(CLIENT_SRC_DIR)/screens/server_browser.c $(SHARED_SRC_DIR)/sim.c $(SHARED_SRC_DIR)/lifecycle.c $(SHARED_SRC_DIR)/connection.c
SERVER_SOURCES := $(SERVER_SRC_DIR)/main.c $(SERVER_SRC_DIR)/logger.c $(SHARED_SRC_DIR)/sim.c $(SHARED_SRC_DIR)/lifecycle.c $(SHARED_SRC_DIR)/connection.c
SHARED_HEADERS := $(SHARED_SRC_DIR)/config.h $(SHARED_SRC_DIR)/vec2.h $(SHARED_SRC_DIR)/world.h $(SHARED_SRC_DIR)/sim.h
SHARED_HEADERS += $(SHARED_SRC_DIR)/protocol.h $(SHARED_SRC_DIR)/lifecycle.h $(SHARED_SRC_DIR)/connection.h
ENET_COMMON_SOURCE_NAMES := callbacks compress host list packet peer protocol
ENET_LINUX_SOURCE_NAMES := unix
ENET_WINDOWS_SOURCE_NAMES := win32
LINUX_ENET_SOURCES := $(addprefix $(ENET_DIR)/,$(addsuffix .c,$(ENET_COMMON_SOURCE_NAMES) $(ENET_LINUX_SOURCE_NAMES)))
WINDOWS_ENET_SOURCES := $(addprefix $(ENET_DIR)/,$(addsuffix .c,$(ENET_COMMON_SOURCE_NAMES) $(ENET_WINDOWS_SOURCE_NAMES)))

LINUX_APP_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
WINDOWS_APP_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(WINDOWS_BUILD_DIR)/%.o,$(CLIENT_SOURCES))
SERVER_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(LINUX_BUILD_DIR)/%.o,$(SERVER_SOURCES))
LINUX_ENET_OBJECTS := $(patsubst $(ENET_DIR)/%.c,$(LINUX_BUILD_DIR)/vendor/enet/%.o,$(LINUX_ENET_SOURCES))
WINDOWS_ENET_OBJECTS := $(patsubst $(ENET_DIR)/%.c,$(WINDOWS_BUILD_DIR)/vendor/enet/%.o,$(WINDOWS_ENET_SOURCES))
SERVER_ENET_OBJECTS := $(LINUX_ENET_OBJECTS)

LINUX_RAYLIB_LIB := $(LINUX_BUILD_DIR)/libraylib.a
WINDOWS_RAYLIB_LIB := $(WINDOWS_BUILD_DIR)/libraylib.a

LINUX_LIBS := -lGL -lm -ldl -lpthread -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lasound
WINDOWS_LIBS := -lopengl32 -lgdi32 -lwinmm -lws2_32

.PHONY: all linux windows server run run-server run-windows docker-server docker-run-server docker-logs devcontainer-up devcontainer-build devcontainer-shell devcontainer-down devcontainer-opencode-sync devcontainer-git-identity devcontainer-github-token devcontainer-github-status clean distclean vendor spec help

all: linux

help:
	@printf '%s\n' \
	  'Targets:' \
	  '  make linux    Build the Linux binary' \
	  '  make run      Build and run the Linux binary' \
	  '  make server   Build the Linux headless server target' \
	  '  make run-server Build and run the Linux headless server target' \
	  '  make spec     Build the LaTeX specification PDF into dist/latex/' \
	  '  make devcontainer-build Build the development container' \
	  '  make devcontainer-up Start the development container' \
	  '  make devcontainer-shell Open a shell in the development container' \
	  '  make devcontainer-down Stop the development container' \
	  '  make devcontainer-opencode-sync Refresh OpenCode state from host into container' \
	  '  make devcontainer-git-identity Copy host git identity into container' \
	  '  make devcontainer-github-token Store GitHub token for container use' \
	  '  make devcontainer-github-status Check GitHub auth inside container' \
	  '  make docker-server Build the server container image' \
	  '  make docker-run-server Build and run the server container' \
	  '  make docker-logs Follow the server container logs' \
	  '  make windows  Build the Windows binary from WSL with mingw-w64' \
	  '  make run-windows Build and run the Windows binary from WSL' \
	  '  make clean    Remove build and dist artifacts' \
	  '  make distclean Remove build artifacts and downloaded raylib source'

linux: $(LINUX_BIN)

run: $(LINUX_BIN)
	./$(LINUX_BIN)

windows: $(WINDOWS_BIN)

server: $(SERVER_BIN)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

spec:
	@$(MKDIR_P) $(SPEC_OUT)
	latexmk -pdf -outdir=$(SPEC_OUT) $(SPEC_SRC)

docker-server:
	$(DOCKER) build -f Dockerfile.server -t $(SERVER_IMAGE) .

docker-run-server: docker-server
	$(DOCKER) run --rm -p 7777:7777/udp $(SERVER_IMAGE)

docker-logs:
	$(DOCKER) logs -f $(SERVER_CONTAINER)

devcontainer-build:
	DOCKER_BUILDKIT=0 $(DOCKER) build -f .devcontainer/Dockerfile -t $(DEVCONTAINER_IMAGE) .

devcontainer-up:
	@mkdir -p "$(DEVCONTAINER_SECRET_DIR)"
	@touch "$(DEVCONTAINER_GITHUB_TOKEN_FILE)"
	@chmod 600 "$(DEVCONTAINER_GITHUB_TOKEN_FILE)"
	$(DOCKER) rm -f $(DEVCONTAINER_CONTAINER) >/dev/null 2>&1 || true
	$(DOCKER) run -d --name $(DEVCONTAINER_CONTAINER) \
		-u dev \
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

run-windows: $(WINDOWS_BIN)
	./$(WINDOWS_BIN)

vendor: $(RAYLIB_DIR) $(RAYGUI_DIR) $(UNITY_DIR)

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

$(LINUX_BIN): $(LINUX_APP_OBJECTS) $(LINUX_RAYLIB_LIB) $(LINUX_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(LINUX_CC) $(LINUX_APP_OBJECTS) $(LINUX_RAYLIB_LIB) $(LINUX_ENET_OBJECTS) -o $@ $(LINUX_LIBS)

$(WINDOWS_BIN): $(WINDOWS_APP_OBJECTS) $(WINDOWS_RAYLIB_LIB) $(WINDOWS_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(WINDOWS_CC) -static $(WINDOWS_APP_OBJECTS) $(WINDOWS_RAYLIB_LIB) $(WINDOWS_ENET_OBJECTS) -o $@ $(WINDOWS_LIBS)

$(SERVER_BIN): $(SERVER_OBJECTS) $(SERVER_ENET_OBJECTS)
	@$(MKDIR_P) $(DIST_DIR)
	$(LINUX_CC) $(SERVER_OBJECTS) $(SERVER_ENET_OBJECTS) -o $@ $(SERVER_LIBS)

$(LINUX_RAYLIB_LIB): $(LINUX_RAYLIB_OBJECTS)
	$(AR) rcs $@ $(LINUX_RAYLIB_OBJECTS)

$(WINDOWS_RAYLIB_LIB): $(WINDOWS_RAYLIB_OBJECTS)
	$(AR) rcs $@ $(WINDOWS_RAYLIB_OBJECTS)

$(LINUX_BUILD_DIR)/raylib/%.o: $(RAYLIB_SRC_DIR)/%.c $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_RAYLIB_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/raylib/%.o: $(RAYLIB_SRC_DIR)/%.c $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_RAYLIB_CFLAGS) -c $< -o $@

$(LINUX_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(LINUX_CFLAGS) -I$(ENET_INCLUDE_DIR) -D_POSIX_C_SOURCE=199309L -D_DEFAULT_SOURCE -c $< -o $@

$(WINDOWS_BUILD_DIR)/client/%.o: $(CLIENT_SRC_DIR)/%.c $(CLIENT_SRC_DIR)/game.h $(CLIENT_SRC_DIR)/net.h $(SHARED_HEADERS) $(RAYLIB_DIR)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(WINDOWS_CFLAGS) -I$(ENET_INCLUDE_DIR) -c $< -o $@

$(LINUX_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c $(SHARED_HEADERS)
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) $(SERVER_CFLAGS) -c $< -o $@

$(LINUX_BUILD_DIR)/server/%.o: $(SERVER_SRC_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

$(LINUX_BUILD_DIR)/vendor/enet/%.o: $(ENET_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(SERVER_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/vendor/enet/%.o: $(ENET_DIR)/%.c
	@$(MKDIR_P) $(dir $@)
	$(WINDOWS_CC) -std=c11 -O2 $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) -I$(ENET_INCLUDE_DIR) -c $< -o $@

# Test targets
TEST_BUILD_DIR := $(BUILD_DIR)/tests
TEST_CFLAGS := -std=c11 -O0 -g $(COMMON_WARNINGS) $(COMMON_INCLUDE_DIRS) $(UNITY_INCLUDE) -DTEST_MODE
TEST_LIBS := -lm

# Find all test files
TEST_SRCS := $(wildcard $(UNIT_TESTS_DIR)/*.c)
TEST_BINS := $(patsubst $(UNIT_TESTS_DIR)/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))

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

$(TEST_BUILD_DIR)/test_lifecycle: $(UNIT_TESTS_DIR)/test_lifecycle.c $(UNITY_SRC) $(SHARED_SRC_DIR)/lifecycle.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_screen: $(UNIT_TESTS_DIR)/test_screen.c $(UNITY_SRC) $(CLIENT_SRC_DIR)/screen.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/test_connection: $(UNIT_TESTS_DIR)/test_connection.c $(UNITY_SRC) $(SHARED_SRC_DIR)/connection.c | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

$(TEST_BUILD_DIR)/%: $(UNIT_TESTS_DIR)/%.c $(UNITY_SRC) | $(UNITY_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LINUX_CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LIBS)

test-coverage: CFLAGS += -fprofile-arcs -ftest-coverage
test-coverage: LDFLAGS += -lgcov
test-coverage: test
	@echo "Generating coverage report..."
	@gcovr -r . --html --html-details -o coverage.html
	@echo "Coverage report generated: coverage.html"

test-clean:
	$(RM_RF) $(TEST_BUILD_DIR) coverage.html *.gcda *.gcno

clean:
	$(RM_RF) $(BUILD_DIR) $(DIST_DIR)

distclean: clean
	$(RM_RF) vendor
