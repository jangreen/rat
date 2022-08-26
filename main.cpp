#include <iostream>
#include "CatParser.h"
#include <antlr4-runtime.h>

using namespace std;
using namespace antlr4;

int main(int argc, const char *argv[]) {
    std::ifstream stream;
    stream.open("input.scene");
    ANTLRInputStream input(stream);

    cout << "End" << endl;
    return 0;
}