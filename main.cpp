#include <unordered_map>
#include "CatInferVisitor.h"
#include "Constraint.h"

using namespace std;

int main(int argc, const char *argv[]) {
    CatInferVisitor visitor;
    ConstraintSet sc = visitor.parse("cat/sc.cat");

    for (const auto &[name, Constraint] : sc) {
        std::cout << name << std::endl;
    }
}