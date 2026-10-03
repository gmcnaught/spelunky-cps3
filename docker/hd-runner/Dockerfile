# Runtime for the official GameMaker Linux runner of Spelunky Classic HD 1.2.2 (refs/hd/linux-arm64, native
# linux/arm64 on Apple Silicon): X11 via Xvfb, GL via Mesa (llvmpipe), audio to ALSA's null device.
#   docker build -t spelunky-hd-runner docker/hd-runner      (scripts/hd_trace.sh builds it when missing)
FROM debian:bookworm-slim
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
      xvfb xauth libgl1 libgl1-mesa-dri libglx-mesa0 libxxf86vm1 libxfixes3 libx11-6 libxrandr2 libxext6 \
      libcurl4 libssl3 libopenal1 libasound2 libpulse0 ca-certificates \
 && rm -rf /var/lib/apt/lists/*
RUN printf 'pcm.!default { type null }\nctl.!default { type hw card 0 }\n' > /etc/asound.conf
ENV LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=dummy DISPLAY=:99 HOME=/tmp
