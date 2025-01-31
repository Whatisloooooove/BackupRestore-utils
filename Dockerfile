FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgtest-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /usr/src/gtest && \
    cd /usr/src/gtest && \
    cmake /usr/src/gtest && \
    make && \
    cp lib/*.a /usr/lib

COPY . /app
WORKDIR /app

RUN rm -rf build && mkdir build && cd build
WORKDIR /app/build
RUN cmake .. && make

CMD ["ctest", "--output-on-failure"]