# syntax=docker/dockerfile:1.7

FROM gcc:15 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --parallel \
 && ctest --test-dir build --output-on-failure

FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/replicator /usr/local/bin/replicator
ENTRYPOINT ["/usr/local/bin/replicator"]
