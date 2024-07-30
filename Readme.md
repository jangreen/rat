# RAT: Relational Algebra (with) Tableaus

RAT is a tool to automaitcally compare weak memory models formalized in CAT.

## Requirements

- [Antlr](https://www.antlr.org)
- [Java](https://www.java.com/de/download/manual.jsp)
- [Graphviz](https://graphviz.org/)
- [Cmake](https://cmake.org/download/)
- [Boost](https://www.boost.org/users/download/)
- [Spdlog](https://github.com/gabime/spdlog)

## Installation

### Docker

The docker image has all dependencies pre-installed to run the tool.
Build and run the container:

```
docker build . -t rat
docker run -w /home/rat -it rat /bin/bash
```

### From Sources

1. Install dependencies above.

   The Antlr tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar and can be installed by:

```
cd /usr/local/lib
curl -O https://www.antlr.org/download/antlr-4.10.1-complete.jar
```

2. Build and the tool

```
cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build
cmake --build ./build --target rat -j 10
```

## Usage

Run the tool

```
./build/rat
```