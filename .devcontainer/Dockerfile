FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    clang-format \
    cppcheck \
    curl \
    docker.io \
    gdb \
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
    npm \
    pkg-config \
    texlive-fonts-recommended \
    texlive-latex-extra \
    texlive-latex-recommended \
    unzip \
    xvfb \
    xz-utils \
    zip \
    && rm -rf /var/lib/apt/lists/*

RUN npm install -g opencode-ai

RUN if id -u vscode >/dev/null 2>&1; then \
      usermod -l dev -d /home/dev -m vscode && \
      groupmod -n dev vscode; \
    fi

USER dev
ENV HOME=/home/dev
