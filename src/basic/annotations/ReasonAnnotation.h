#pragma once

#include "../Relation.h"
#include "../Set.h"
#include "../model/InterpretationTree.h"
#include "LeafAnnotated.h"
#include "LeafAnnotation.h"

/**
 * Reason annotations can be more or less precise:
 * - annotate edges in given spurious counterexample
 * - annotate full lhs of assumption
 * Thus, annotations can differ in disjunctive choices (unions, transitive closure)
 * example: (a | b) <= c, ~c, a
 *      you can either annotate ~c with 'a' or '(a | b)'
 * For transitive closures, annotating the concrete counterexample may diverge
 *
 * Question: Why do most example can be solved using concrete annotations? (depends on where we
 * annotate the proof)
 *
 * Currently we annotate the flattened lhs of the asssumptions?
 */

typedef CanonicalExpression Reason;
typedef ExprSet Reasons;

template <>
struct std::hash<Reasons> {
  std::size_t operator()(const Reasons &annotation) const noexcept;
};

CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(CanonicalRelation relation,
                                                       const InterpretationPtr &interpretation,
                                                       const Edge &tracedValue);
CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(CanonicalSet set,
                                                       const InterpretationPtr &interpretation,
                                                       const Event &tracedValue);
CanonicalLeafAnnotation<Reasons> annotateReasons(CanonicalSet set,
                                                 const InterpretationPtr &interpretation);

LeafAnnotatedSet<Reasons> substitute(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                     CanonicalSet search, CanonicalSet replace, int *n);
LeafAnnotatedSet<Reasons> substituteAll(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                        CanonicalSet search, CanonicalSet replace);
LeafAnnotatedRelation<Reasons> substituteAll(
    const LeafAnnotatedRelation<Reasons> &annotatedRelation, CanonicalRelation search,
    CanonicalRelation replace);
LeafAnnotatedSet<Reasons> substituteAll(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                        CanonicalRelation search, CanonicalRelation replace);

[[nodiscard]] bool validateReasons(const LeafAnnotatedSet<Reasons> &annotatedSet);
[[nodiscard]] bool validateReasons(const LeafAnnotatedRelation<Reasons> &annotatedRelation);
