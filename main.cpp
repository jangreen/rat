#include <iostream>
#include <unordered_map>
#include <antlr4-runtime.h>
#include <CatParser.h>
#include <CatLexer.h>
#include "CatInferVisitor.h"
#include "Constraint.h"

using namespace std;
using namespace antlr4;

int main(int argc, const char *argv[]) {
    ifstream stream;
    stream.open("cat/sc.cat");
    ANTLRInputStream input(stream);

    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    CatParser::McmContext* scContext = parser.mcm();
    CatInferVisitor visitor;
    //any sc = visitor.visitMcm(scContext);
    ConstraintSet sc = any_cast<ConstraintSet>(visitor.visitMcm(scContext));
    for (const auto &[name, Constraint] : sc) {
        std::cout << name << std::endl;
    }
    // RuleContext context = (RuleContext) sc->accept(nullptr);
    // *sc->definition().

    // cout << ->toStringTree() << endl;
    stream.close();
    return 0;
}