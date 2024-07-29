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
std::optional<PartialDNF> applyRule(const Literal &context, const AnnotatedSet &annotatedSet);
std::optional<AnnotatedSet> saturateBase(const AnnotatedSet &annotatedSet);
std::optional<AnnotatedSet> saturateBaseSet(const AnnotatedSet &annotatedSet);
std::optional<AnnotatedSet> saturateId(const AnnotatedSet &annotatedSet);
// ---------------------- RELATIONAL RULES ----------------------
std::optional<PartialDNF> applyRelationalRule(const Literal &context,
                                              const AnnotatedSet &annotatedSet);

// ---------------------- MODAL RULES ----------------------
std::optional<DNF> applyPositiveModalRule(const Literal &literal, int minimalEvent);
std::optional<PartialDNF> applyPositiveModalRule(const AnnotatedSet &annotatedSet,
                                                 int minimalEvent);

// TODO: give better name
PartialDNF substituteIntersectionOperand(bool substituteRight, const PartialDNF &disjunction,
                                         const AnnotatedSet &otherOperand);

static int saturationBound = 1;
inline bool lastRuleWasUnrolling = false;
}  // namespace Rules