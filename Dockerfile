# Download base image
FROM ubuntu:24.04

# Prevent interactive scripts from getting stuck
ARG DEBIAN_FRONTEND=noninteractive

# Set working directory
WORKDIR home

# Install dependencies
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y \
    git \
    curl \
    wget \
    cmake \
    clang \
    default-jre \
    libspdlog-dev \
    graphviz

# Install boost ('apt-get install libboost-all-dev' does not install latest version)
RUN cd /usr/local && \
    wget https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.gz && \
    tar -xf boost_1_85_0.tar.gz && \
    cd boost_1_85_0 && \
    ./bootstrap.sh && \
    ./b2 install --prefix=/usr/

# Install Antlr
RUN cd /usr/local/lib && \
    curl -O https://www.antlr.org/download/antlr-4.10.1-complete.jar

# Install RAT tool
RUN git clone --branch master https://github.com/jangreen/rat.git && \
    cd rat && \
    cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build && \
    cmake --build ./build --target rat -j 10
