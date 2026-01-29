# Download base image
# Newer versions of Kater reject files using po-imm, fr-imm, and similar relations
FROM genmc/kater:v0.2

# Prevent interactive scripts from getting stuck
ARG DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y \
    git \
    curl \
    wget \
    cmake \
    default-jre \
    libspdlog-dev \
    graphviz \
    time

#ENV CC=clang
#ENV CXX=clang++

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
RUN cd /root && \
    git clone --branch oopsla26 https://github.com/jangreen/rat.git && \
    cd rat && \
    cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build && \
    cmake --build ./build --target rat -j 10


