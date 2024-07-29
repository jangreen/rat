# RAT: Relational Algebra (with) Tableaus

RAT is a tool to automaitcally compare weak memory models formalized in CAT.

## Requirements
- [Antlr](https://www.antlr.org)
- [Graphviz](https://graphviz.org/)

## Installation
Antlr tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar
(Antlr runtime will be installed and linked by cmake)

```
cd /usr/local/lib
curl -O https://www.antlr.org/download/antlr-4.10.1-complete.jar
```

Install Graphviz

`$ brew install graphviz`

Install brew, spdl with homebrew

## Usage

/opt/homebrew/bin/cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ -S/Users/jangruenke/cat-infer -B/Users/jangruenke/cat-infer/build -G "Unix Makefiles"

/opt/homebrew/bin/cmake --build /Users/jangruenke/cat-infer/build --config RelWithDebInfo --target all -j 14 --
