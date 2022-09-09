#include "Constraint.h"

using namespace std;

Constraint::Constraint() {}
Constraint::Constraint(const ConstraintType &type, Relation *relation, const string &name) : type(type), relation(relation), name(name) {}
Constraint::~Constraint() {}
