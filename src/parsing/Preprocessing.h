#pragma once
#include <map>
#include <unordered_set>

#include "../basic/Literal.h"
#include "../helper/utility.h"
#include "Preprocessing.h"

typedef std::map<CanonicalRelation, std::unordered_set<CanonicalRelation>> CanonicalParents;
typedef std::map<std::string, std::vector<CanonicalRelation>> ReplaceMap;

namespace Preprocessing {
void preprocessing(Cube& goal);

void updateParentMap(CanonicalRelation relation, CanonicalParents& parentMap);
void updateParentMap(CanonicalSet set, CanonicalParents& parentMap);
// is pure iff it contains no base relations that could be saturated
bool isPure(CanonicalRelation relation);
ReplaceMap greatestCommonConjunctiveContext(const Cube& goal);
void eleminateRedundantConjunctiveContexts(Literal& literal, const ReplaceMap& commonContexts);
void eleminateRedundantConjunctiveContexts(Cube& goal);

void getBase(CanonicalRelation relation, std::unordered_set<CanonicalExpression>& baseExpresssions);
void getBase(CanonicalSet set, std::unordered_set<CanonicalExpression>& baseExpresssions);
// overapproximates expressions that may not be empty
// currently we only consider base relations/base sets
std::unordered_set<CanonicalExpression> nonEmptyExpressions(const Cube& goal);
void replaceEmptyExpressionsInNegatedLiterals(
    Literal& literal, const std::unordered_set<CanonicalExpression>& nonEmpty);
void replaceEmptyExpressionsInNegatedLiterals(Cube& goal);

}  // namespace Preprocessing
