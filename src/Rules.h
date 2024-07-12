#pragma once

#include "Annotated.h"
#include "Assumption.h"
#include "Literal.h"

namespace Rules {
// ---------------------- LITERAL RULES ----------------------
std::optional<DNF> applyRule(const Literal &literal, bool modalRules);
std::optional<DNF> handleIntersectionWithEvent(const Literal &literal, bool modalRules);
std::optional<Literal> saturateBase(const Literal &literal);
std::optional<Literal> saturateId(const Literal &literal);
// ---------------------- SET RULES ----------------------
std::optional<PartialDNF> applyRule(const Literal &context, const AnnotatedSet &annotatedSet,
                                    bool modalRules);
std::optional<AnnotatedSet> saturateBase(const AnnotatedSet &annotatedSet);
std::optional<AnnotatedSet> saturateId(const AnnotatedSet &annotatedSet);
// ---------------------- RELATIONAL RULES ----------------------
std::optional<PartialDNF> applyRelationalRule(const Literal &context,
                                              const AnnotatedSet &annotatedSet, bool modalRules);

// TODO: give better name
PartialDNF substituteIntersectionOperand(bool substituteRight, const PartialDNF &disjunction,
                                         const AnnotatedSet &otherOperand);

static int saturationBound = 1;
inline bool lastRuleWasUnrolling = false;
}  // namespace Rules