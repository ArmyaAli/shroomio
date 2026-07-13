FROM node:22-bookworm-slim AS node-runtime

FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

COPY --from=node-runtime /usr/local/ /usr/local/

ARG ACTIONLINT_VERSION=1.7.7

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    clang-format \
    cppcheck \
    curl \
    docker.io \
    gdb \
    gcovr \
    gh \
    latexmk \
    libasound2-dev \
    libgl1-mesa-dev \
    libsqlite3-dev \
    lmodern \
    libx11-dev \
    libxcursor-dev \
    libxi-dev \
    libxinerama-dev \
    libxrandr-dev \
    make \
    mingw-w64 \
    ninja-build \
    pkg-config \
    ruby \
    tar \
    texlive-fonts-recommended \
    texlive-latex-extra \
    texlive-latex-recommended \
    unzip \
    valgrind \
    xvfb \
    xz-utils \
    yamllint \
    zip \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    arch="$(dpkg --print-architecture)"; \
    case "$arch" in \
      amd64) actionlint_arch="amd64" ;; \
      arm64) actionlint_arch="arm64" ;; \
      *) echo "Unsupported actionlint architecture: $arch" >&2; exit 1 ;; \
    esac; \
    curl -fsSL "https://github.com/rhysd/actionlint/releases/download/v${ACTIONLINT_VERSION}/actionlint_${ACTIONLINT_VERSION}_linux_${actionlint_arch}.tar.gz" \
      | tar -xz -C /usr/local/bin actionlint; \
    chmod +x /usr/local/bin/actionlint

RUN npm install -g opencode-ai

RUN if id -u vscode >/dev/null 2>&1; then \
      usermod -l dev -d /home/dev -m vscode && \
      groupmod -n dev vscode; \
    fi

USER dev
ENV HOME=/home/dev
