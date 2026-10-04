# Headless MAME for the project's MAME scripts (no host window, no focus change): scripts/mame.sh.
#   docker build -t spelunky-mame:latest docker/mame
FROM debian:sid-slim
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends mame \
    && rm -rf /var/lib/apt/lists/*
ENV SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
