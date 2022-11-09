# CAT Infer

## Requirements

### Parsing CAT files: Antlr

Antlr tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar
(Antlr runtime will be installed and linked by cmake)

$ cd /usr/local/lib
$ curl -O <https://www.antlr.org/download/antlr-4.10.1-complete.jar>

### Rendering proofs: Graphviz

Install Graphviz. For Mac:

$ brew install graphviz

-> to enable dot live preview of proofs: open dot preview(extension) on right side together with proof.dot file (configure debounce rendering in settings)

## Useful commands

$ cmake -S . -B build         # generate Makefile
$ cmake --build build         # build using Makefile
$ cmake --install build       # install
$ ./build/CatInfer            # run program
