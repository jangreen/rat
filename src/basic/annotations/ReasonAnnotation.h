#pragma once

#include "../Relation.h"
#include "../Set.h"
#include "../model/InterpretationTree.h"
#include "LeafAnnotated.h"
#include "LeafAnnotation.h"

typedef ExprSet Reasons;

template <>
struct std::hash<Reasons> {
  std::size_t operator()(const Reasons &annotation) const noexcept;
};

CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(
    CanonicalRelation relation, const InterpretationPtr &interpretation, const Edge &tracedValue);
CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(
    CanonicalSet set, const InterpretationPtr &interpretation, const Event &tracedValue);
CanonicalLeafAnnotation<Reasons> annotateReasons(
    CanonicalSet set, const InterpretationPtr &interpretation);

LeafAnnotatedSet<Reasons> substitute(
    const LeafAnnotatedSet<Reasons> &annotatedSet, CanonicalSet search,
    CanonicalSet replace, int *n);
LeafAnnotatedSet<Reasons> substituteAll(
    const LeafAnnotatedSet<Reasons> &annotatedSet, CanonicalSet search,
    CanonicalSet replace);
LeafAnnotatedRelation<Reasons> substituteAll(
    const LeafAnnotatedRelation<Reasons> &annotatedRelation, CanonicalRelation search,
    CanonicalRelation replace);
LeafAnnotatedSet<Reasons> substituteAll(
    const LeafAnnotatedSet<Reasons> &annotatedSet, CanonicalRelation search,
    CanonicalRelation replace);

[[nodiscard]] bool validateReasons(
    const LeafAnnotatedSet<Reasons> &annotatedSet);
[[nodiscard]] bool validateReasons(
    const LeafAnnotatedRelation<Reasons> &annotatedRelation);
