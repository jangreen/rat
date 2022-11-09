Install Antlr4

For mac: 'brew install antlr'    available command: antlr -Dlanguage=Cpp -o ./antlr ./cat/Cat.g4
'brew install antlr4-cpp-runtime'

Runtime is not usable install manually

-----------------
START HERE:

(runtime gets installed?/updated by makefile)
tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar
$ cd /usr/local/lib
$ curl -O <https://www.antlr.org/download/antlr-4.10.1-complete.jar>

To see dot live preview: open dot preview on right side tohgether with proof.dot file

Commands
cmake -S . -B build         # generate Makefile
cmake --build build         # build using Makefile
cmake --install build       # install
./build/CatInfer            # run program
