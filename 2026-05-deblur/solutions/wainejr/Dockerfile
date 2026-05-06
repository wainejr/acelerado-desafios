FROM debian:bookworm-slim AS build
ARG ZIG_VERSION=0.15.1
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates wget xz-utils \
    && wget -qO- https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz \
       | tar -xJ -C /opt \
    && mv /opt/zig-x86_64-linux-${ZIG_VERSION} /opt/zig \
    && rm -rf /var/lib/apt/lists/*
ENV PATH="/opt/zig:${PATH}"
WORKDIR /src
COPY build.zig solution.zig ./
RUN zig build -Doptimize=ReleaseSafe

FROM debian:bookworm-slim
COPY --from=build /src/zig-out/bin/solution /app/solution
CMD ["/app/solution"]
