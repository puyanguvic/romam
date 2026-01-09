FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake iproute2 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /work
COPY . /work

RUN cmake -S standalone -B build-standalone && cmake --build build-standalone -j

ENTRYPOINT ["/work/build-standalone/daemon/romamd"]

