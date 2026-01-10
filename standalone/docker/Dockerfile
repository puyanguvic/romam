FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY standalone /src/standalone

RUN cmake -S /src/standalone -B /build \
  && cmake --build /build -j \
  && install -d /out \
  && install -m 0755 /build/daemon/romamd /out/romamd \
  && install -m 0755 /build/traffic/romam-traffic /out/romam-traffic

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    iproute2 \
  && rm -rf /var/lib/apt/lists/*

COPY --from=build /out/romamd /usr/local/bin/romamd
COPY --from=build /out/romam-traffic /usr/local/bin/romam-traffic

ENTRYPOINT ["/usr/local/bin/romamd"]
