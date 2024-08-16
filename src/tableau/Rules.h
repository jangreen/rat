#pragma once

#include "../Assumption.h"
#include "../basic/Annotated.h"
#include "../basic/Literal.h"

namespace Rules {
// ---------------------- LITERAL RULES ----------------------
std::optional<DNF> applyRule(const Literal &literal);
std::optional<DNF> handleIntersectionWithEvent(const Literal &literal);
std::optional<Literal> saturateBase(const Literal &literal);
std::optional<Literal> saturateBaseSet(const Literal &literal);
std::optional<Literal> saturateId(const Literal &literal);
// ---------------------- SET RULES ----------------------
std::optional<PartialDNF> applyRule(const Literal &context,
                                    const SaturationAnnotatedSet &annotatedSet);
std::optional<SaturationAnnotatedSet> saturateBase(const SaturationAnnotatedSet &annotatedSet);
std::optional<SaturationAnnotatedSet> saturateBaseSet(const SaturationAnnotatedSet &annotatedSet);
std::optional<SaturationAnnotatedSet> saturateId(const SaturationAnnotatedSet &annotatedSet);
// ---------------------- RELATIONAL RULES ----------------------
std::optional<PartialDNF> applyRelationalRule(const Literal &context,
                                              const SaturationAnnotatedSet &annotatedSet);

// ---------------------- MODAL RULES ----------------------
std::optional<DNF> applyPositiveModalRule(const Literal &literal, int minimalEvent);
std::optional<PartialDNF> applyPositiveModalRule(const SaturationAnnotatedSet &annotatedSet,
                                                 int minimalEvent);

// TODO: give better name
PartialDNF substituteIntersectionOperand(bool substituteRight, const PartialDNF &disjunction,
                                         const SaturationAnnotatedSet &otherOperand);

inline bool lastRuleWasUnrolling = false;
}  // namespace Rules