#include "Assumption.h"

using namespace std;

Assumption::Assumption(const AssumptionType type, shared_ptr<Relation> relation, optional<string> baseRelation) : type(type), relation(relation), baseRelation(baseRelation) {}
