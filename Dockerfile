# ============================================================================
# Kaikai Headless Server - Docker Container
# ============================================================================
# Build:  docker build -t kaikai-server .
# Run:    docker run -d -p 7777:7777/udp -p 7777:7777/tcp kaikai-server
# Test:   Connect your client to <docker-host-ip>:7777
# ============================================================================

FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source
COPY . .

# Build headless server
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target kaikai_headless_server -j$(nproc)

# ---- Runtime image ----
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the built binary
COPY --from=builder /build/build/kaikai_headless_server .

# ENet uses UDP by default, but also needs TCP for connection setup
EXPOSE 7777/udp
EXPOSE 7777/tcp

# Run the server
CMD ["./kaikai_headless_server"]
