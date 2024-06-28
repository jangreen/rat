#pragma once

#include "Annotation.h"
#include "Assumption.h"
#include "Literal.h"

namespace Rules {
// ---------------------- LITERAL RULES ----------------------
std::optional<DNF> applyRule(const Literal &literal, const bool modalRules);
std::optional<DNF> handleIntersectionWithEvent(const Literal &literal, const bool modalRules);
std::optional<Literal> saturateBase(const Literal &literal);
std::optional<Literal> saturateId(const Literal &literal);
// ---------------------- SET RULES ----------------------
std::optional<PartialDNF> applyRule(const Literal &context, AnnotatedSet annotatedSet,
                                    const bool modalRules);
std::optional<AnnotatedSet> saturateBase(const AnnotatedSet annotatedSet);
std::optional<AnnotatedSet> saturateId(const AnnotatedSet annotatedSet);
// ---------------------- RELATIONAL RULES ----------------------
std::optional<PartialDNF> applyRelationalRule(const Literal &context, AnnotatedSet annotatedSet,
                                              const bool modalRules);

// TODO: give better name
PartialDNF substituteIntersectionOperand(const bool substituteRight, const PartialDNF &disjunction,
                                         const AnnotatedSet otherOperand);

}  // namespace Rules