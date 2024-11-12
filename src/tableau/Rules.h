#pragma once

#include "../basic/Literal.h"

namespace Rules {
// ---------------------- LITERAL RULES ----------------------
std::optional<DNF> applyRule(const Literal &literal);
std::optional<DNF> handleIntersectionWithEvent(const Literal &literal);
Cube saturate(const Literal &literal);
// Cube saturateBaseSet(const Literal &literal);
// Cube saturateId(const Literal &literal);
// ---------------------- SET RULES ----------------------
std::optional<PartialDNF> applyRule(const Literal &context,
                                    const LeafAnnotatedSet<Reasons> &annotatedSet);
std::vector<LeafAnnotatedSet<Reasons>> saturateBase(const LeafAnnotatedSet<Reasons> &annotatedSet);
std::vector<LeafAnnotatedSet<Reasons>> saturateBaseSet(
    const LeafAnnotatedSet<Reasons> &annotatedSet);
// std::vector<LeafAnnotatedSet<Reasons>> saturateId(const
// LeafAnnotatedSet<Reasons> &annotatedSet);
// ---------------------- RELATIONAL RULES ----------------------
std::optional<PartialDNF> applyRelationalRule(const Literal &context,
                                              const LeafAnnotatedSet<Reasons> &annotatedSet);

// ---------------------- MODAL RULES ----------------------
std::optional<DNF> applyPositiveModalRule(const Literal &literal, int minimalEvent);
std::optional<PartialDNF> applyPositiveModalRule(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                                 int minimalEvent);

PartialDNF substituteIntersectionOperand(bool substituteRight, const PartialDNF &disjunction,
                                         const LeafAnnotatedSet<Reasons> &otherOperand);

inline bool lastRuleWasUnrolling = false;
}  // namespace Rules