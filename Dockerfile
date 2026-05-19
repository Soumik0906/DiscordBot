# Build stage
FROM alpine:latest AS builder

RUN apk add --no-cache \
    cmake \
    make \
    g++ \
    git \
    openssl-dev \
    zlib-dev \
    linux-headers

# Build and install D++ from source
WORKDIR /tmp/dpp
RUN git clone https://github.com/brainboxdotcc/DPP.git . && \
    cmake -B build \
    -DDPP_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    cmake --install build

# Set up project
WORKDIR /app
COPY . .

# Build the bot
RUN mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Runtime stage
FROM alpine:latest

# Install only essential runtime dependencies
RUN apk add --no-cache \
    libstdc++ \
    openssl \
    zlib

WORKDIR /app

# Copy the binary and D++ libraries from the builder
COPY --from=builder /app/build/discord-bot .
COPY --from=builder /usr/local/lib/libdpp.so* /usr/lib/

# Run the bot
CMD ["./discord-bot"]
