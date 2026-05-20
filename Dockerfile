# Build stage
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    make \
    g++ \
    wget \
    libopus-dev \
    libsodium-dev \
    libssl-dev \
    ca-certificates \
    libpqxx-dev

# Download and install precompiled D++ deb package
WORKDIR /tmp
RUN wget -q https://github.com/brainboxdotcc/DPP/releases/download/v10.1.4/libdpp-10.1.4-linux-x64.deb && \
    dpkg -i libdpp-10.1.4-linux-x64.deb

# Set up project
WORKDIR /app
COPY . .

# Build the bot
RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)


# Runtime stage
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies and precompiled D++ deb package
RUN apt-get update && apt-get install -y \
    wget \
    libopus0 \
    libsodium23 \
    openssl \
    ca-certificates \
    libpqxx-7.8t64 \
    libpq5 \
    tzdata \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp
RUN wget -q https://github.com/brainboxdotcc/DPP/releases/download/v10.1.4/libdpp-10.1.4-linux-x64.deb && \
    dpkg -i libdpp-10.1.4-linux-x64.deb && \
    rm libdpp-10.1.4-linux-x64.deb

WORKDIR /app

# Copy the built bot binary
COPY --from=builder /app/build/discord-bot .

# Run the bot
CMD ["./discord-bot"]
