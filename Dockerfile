# This is a Dockerfile for fivegd.
FROM debian:bionic

# Install required system packages
RUN apt-get update && apt-get install -y \
    automake \
    bsdmainutils \
    curl \
    g++ \
    libboost-all-dev \
    libevent-dev \
    libssl-dev \
    libtool \
    libzmq3-dev \
    make \
    openjdk-8-jdk \
    pkg-config \
    zlib1g-dev

# Install Berkeley DB 4.8
RUN curl -L http://download.oracle.com/berkeley-db/db-4.8.30.tar.gz | tar -xz -C /tmp && \
    cd /tmp/db-4.8.30/build_unix && \
    ../dist/configure --enable-cxx --includedir=/usr/include/bdb4.8 --libdir=/usr/lib && \
    make -j$(nproc) && make install && \
    cd / && rm -rf /tmp/db-4.8.30

# Install minizip from source (unavailable from apt on Ubuntu 14.04)
RUN curl -L https://www.zlib.net/zlib-1.2.11.tar.gz | tar -xz -C /tmp && \
    cd /tmp/zlib-1.2.11/contrib/minizip && \
    autoreconf -fi && \
    ./configure --enable-shared=no --with-pic && \
    make -j$(nproc) install && \
    cd / && rm -rf /tmp/zlib-1.2.11

# Install zmq from source (outdated version from apt on Ubuntu 14.04)
RUN curl -L https://github.com/zeromq/libzmq/releases/download/v4.3.1/zeromq-4.3.1.tar.gz | tar -xz -C /tmp && \
    cd /tmp/zeromq-4.3.1/ && ./configure --disable-shared --without-libsodium --with-pic && \
    make -j$(nproc) install && \
    cd / && rm -rf /tmp/zeromq-4.3.1/

# Create user to run daemon
RUN useradd -m -U fivegd

# Build Fiveg
COPY . /tmp/fiveg/

RUN cd /tmp/fiveg && \
    ./autogen.sh && \
    ./configure --without-gui --prefix=/usr && \
    make -j$(nproc) && \
    make check && \
    make install && \
    cd / && rm -rf /tmp/fiveg

# Remove unused packages
RUN apt-get remove -y \
    automake \
    bsdmainutils \
    curl \
    g++ \
    libboost-all-dev \
    libevent-dev \
    libssl-dev \
    libtool \
    libzmq3-dev \
    make

# Start Fiveg Daemon
USER fivegd

RUN mkdir /home/fivegd/.fiveg
VOLUME [ "/home/fivegd/.fiveg" ]

# Main network ports
EXPOSE 22020
EXPOSE 22019

# Test network ports
EXPOSE 41998
EXPOSE 41999

# Regression test network ports
EXPOSE 40998
EXPOSE 40999

ENTRYPOINT [ "/usr/bin/fivegd" ]
