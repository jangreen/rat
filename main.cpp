#include <iostream>
#include <antlr4-runtime.h>
#include <CatParser.h>
#include <CatLexer.h>

using namespace std;
using namespace antlr4;

int main(int argc, const char *argv[]) {
    ifstream stream;
    stream.open("cat/sc.cat");
    ANTLRInputStream input(stream);

    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    cout << parser.mcm()->toStringTree() << endl;
    stream.close();
    return 0;
}