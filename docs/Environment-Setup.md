# Environment Setup

This guide covers setting up a development environment for shroomio on Linux.

## Prerequisites

### Native Linux

Install build dependencies:

```bash
sudo apt update
sudo apt install build-essential curl mingw-w64 \
  libx11-dev libxcursor-dev libxrandr-dev libxi-dev \
  libgl1-mesa-dev libasound2-dev \
  latexmk texlive-latex-recommended texlive-latex-extra
```

- `build-essential`: gcc, make, and core toolchain
- `curl`: downloads raylib source on demand during `make`
- `mingw-w64`: cross-compiler for Windows builds
- `libx11-dev` et al.: raylib Linux desktop dependencies
- `latexmk`, `texlive-*`: builds the specification PDF

### Docker (recommended)

Install Docker Engine and Docker Compose:

```bash
# Docker Engine
curl -fsSL https://get.docker.com | sudo sh
sudo usermod -aG docker $USER

# Docker Compose (standalone or plugin)
sudo apt install docker-compose-plugin
```

### GitHub CLI

```bash
# Via apt
type -p curl >/dev/null || sudo apt install curl -y
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt update
sudo apt install gh -y
```

Authenticate:

```bash
gh auth login
```

## Development Container (VS Code)

The repo includes a `.devcontainer/` configuration for VS Code:

1. Open the repository in VS Code
2. When prompted, click **Reopen in Container**
3. VS Code will build the devcontainer image with all dependencies pre-installed

The devcontainer includes:
- C11 toolchain (`build-essential`)
- Linux desktop build dependencies for raylib
- `mingw-w64` for `make client-windows`
- Docker CLI support
- GitHub CLI (`gh`)
- Latest opencode via npm

### Devcontainer Without VS Code

You can use the devcontainer with plain Docker:

```bash
make devcontainer-build    # Build the image
make devcontainer-up       # Start the container
make devcontainer-shell    # Open a shell inside
make devcontainer-down     # Stop the container
```

### Host Integration

The devcontainer mounts your host:
- `~/.ssh/` (read-only) for git SSH access
- `~/.gitconfig` (read-only) for git identity
- `~/.config/opencode/` and related dirs for opencode state

To set a persistent GitHub token inside the container:

```bash
make devcontainer-github-token
make devcontainer-down
make devcontainer-up
make devcontainer-github-status
```

## Verifying Your Setup

```bash
# Clone the repo
git clone git@github.com:ArmyaAli/shroomio.git
cd shroomio

# Build the client
make client-linux

# Build the server
make server-linux

# Build the Windows client
make client-windows

# Build the spec PDF
latexmk -pdf -output-directory=. design/shroomio-specification.tex
```

All binaries land in `dist/` and the spec PDF at the repo root.
