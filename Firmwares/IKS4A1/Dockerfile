FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project
COPY . .

RUN make clean && make
