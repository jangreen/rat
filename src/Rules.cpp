#include "Rules.h"

#include <spdlog/spdlog.h>

#include "utility.h"

std::optional<PartialDNF> Rules::applyRelationalRule(const Literal& context,
                                                     const AnnotatedSet& annotatedSet,
                                                     const bool modalRules

) {
  // e.r
  const CanonicalSet set = std::get<CanonicalSet>(annotatedSet);
  const CanonicalAnnotation setAnnotation = std::get<CanonicalAnnotation>(annotatedSet);
  const CanonicalSet event = set->leftOperand;
  const CanonicalRelation relation = set->relation;
  const CanonicalAnnotation relationAnnotation = setAnnotation->getRight();
  const SetOperation operation = set->operation;
  // operation indicates if left or right rule is used
  // in this function all variable names reflect the case for SetOperation::image
  assert(operation == SetOperation::image || operation == SetOperation::domain);

  switch (relation->operation) {
    case RelationOperation::baseRelation: {
      // only use if want to
      if (!modalRules) {
        return std::nullopt;
      }
      if (context.negated) {
        // Rule (~aL), Rule (~aR): requires context (handled later)
        return std::nullopt;
      }

      // LeftRule: [e.b] -> { [f], (e,f) \in b }
      // -> Rule (aL)
      // RightRule: [b.e] -> { [f], (f,e) \in b }
      // -> Rule (aR)
      CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
      const auto& b = *relation->identifier;
      const int first = operation == SetOperation::image ? *event->label : *f->label;
      const int second = operation == SetOperation::image ? *f->label : *event->label;

      return PartialDNF{{AnnotatedSet(f, Annotation::none()), Literal(first, second, b)}};
    }
    case RelationOperation::cartesianProduct:
      // TODO: implement
      spdlog::error("[Error] Cartesian products are currently not supported.");
      assert(false);
    case RelationOperation::relationUnion: {
      // LeftRule:
      //    Rule (~v_2L): ~[e.(r1 | r2)] -> { ~[e.r1], ~[e.r2] }
      //    Rule (v_2L): [e.(r1 | r2)] -> { [e.r1] }, { [e.r2] }
      // RightRule: [b.e] -> { [f], (f,e) \in b }
      //    Rule (~v_2R): ~[(r1 | r2).e] -> { ~[r1.e], ~[r2.e] }
      //    Rule (v_2R):
      CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);

      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(er1, Annotation::none())},
                          {AnnotatedSet(er2, Annotation::none())}};
      }

      auto er1_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getLeft());
      auto er2_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getRight());
      return PartialDNF{{AnnotatedSet(er1, er1_annotation), AnnotatedSet(er2, er2_annotation)}};
    }
    case RelationOperation::composition: {
      // Rule (._22L): [e(a.b)] -> { [(e.a)b] }
      // Rule (._22R): [(b.a)e] -> { [b(a.e)] }
      auto a = operation == SetOperation::image ? relation->leftOperand : relation->rightOperand;
      auto b = operation == SetOperation::image ? relation->rightOperand : relation->leftOperand;
      CanonicalSet ea = Set::newSet(operation, event, a);
      CanonicalSet ea_b = Set::newSet(operation, ea, b);

      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(ea_b, Annotation::none())}};
      }

      auto a_annotation = operation == SetOperation::image ? relationAnnotation->getLeft()
                                                           : relationAnnotation->getRight();
      auto b_annotation = operation == SetOperation::image ? relationAnnotation->getLeft()
                                                           : relationAnnotation->getRight();
      auto ea_annotation = Annotation::newAnnotation(Annotation::none(), a_annotation);
      auto ea_b_annotation = Annotation::newAnnotation(ea_annotation, b_annotation);
      return PartialDNF{{AnnotatedSet(ea_b, ea_b_annotation)}};
    }
    case RelationOperation::converse: {
      // Rule (^-1L): [e.(r^-1)] -> { [r.e] }
      const SetOperation oppositOp =
          operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
      CanonicalSet re = Set::newSet(oppositOp, event, relation->leftOperand);

      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(re, Annotation::none())}};
      }

      auto re_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getLeft());
      return PartialDNF{{AnnotatedSet(re, re_annotation)}};
    }
    case RelationOperation::emptyRelation: {
      // Rule (\bot_2L), (\bot_2R) + (\bot_1) -> FALSE
      return context.negated ? PartialDNF{{TOP}} : PartialDNF{{BOTTOM}};
    }
    case RelationOperation::fullRelation: {
      // TODO: implement
      // Rule (\top_2\lrule):
      spdlog::error("[Error] Full relations are currently not supported.");
      assert(false);
    }
    case RelationOperation::idRelation: {
      // Rule (\id\lrule): [e.id] -> { [e] }
      return PartialDNF{{AnnotatedSet(event, Annotation::none())}};
    }
    case RelationOperation::relationIntersection: {
      // Rule (\cap_2\lrule): [e.(r1 & r2)] -> { [e.r1 & e.r2] }
      CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      CanonicalSet er1_and_er2 = Set::newSet(SetOperation::setIntersection, er1, er2);

      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(er1_and_er2, Annotation::none())}};
      }

      auto er1_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getLeft());
      auto er2_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getRight());
      auto er1_and_er2_annotation = Annotation::newAnnotation(er1_annotation, er2_annotation);
      return PartialDNF{{AnnotatedSet(er1_and_er2, er1_and_er2_annotation)}};
    }
    case RelationOperation::transitiveClosure: {
      // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
      // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
      CanonicalSet er = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet err_star = Set::newSet(operation, er, relation);

      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(err_star, Annotation::none())},
                          {AnnotatedSet(event, Annotation::none())}};
      }

      auto er_annotation =
          Annotation::newAnnotation(Annotation::none(), relationAnnotation->getLeft());
      auto err_star_annotation = Annotation::newAnnotation(er_annotation, relationAnnotation);
      return PartialDNF{
          {AnnotatedSet(err_star, err_star_annotation), AnnotatedSet(event, Annotation::none())}};
    }
    default:
      throw std::logic_error("unreachable");
  }
  throw std::logic_error("unreachable");
}

PartialDNF Rules::substituteIntersectionOperand(const bool substituteRight,
                                                const PartialDNF& disjunction,
                                                const AnnotatedSet& otherOperand) {
  std::vector<std::vector<PartialLiteral>> resultDisjunction;
  resultDisjunction.reserve(disjunction.size());
  for (const auto& conjunction : disjunction) {
    std::vector<PartialLiteral> resultConjunction;
    resultConjunction.reserve(conjunction.size());
    for (const auto& literal : conjunction) {
      if (std::holds_alternative<Literal>(literal)) {
        auto p = std::get<Literal>(literal);
        resultConjunction.emplace_back(p);
      } else {
        auto as = std::get<AnnotatedSet>(literal);
        auto s = std::get<CanonicalSet>(as);
        auto a = std::get<CanonicalAnnotation>(as);
        auto os = std::get<CanonicalSet>(otherOperand);
        auto oa = std::get<CanonicalAnnotation>(otherOperand);
        // substitute
        s = substituteRight ? Set::newSet(SetOperation::setIntersection, os, s)
                            : Set::newSet(SetOperation::setIntersection, s, os);
        a = substituteRight ? Annotation::newAnnotation(oa, a) : Annotation::newAnnotation(a, oa);

        resultConjunction.emplace_back(AnnotatedSet(s, a));
      }
    }
    resultDisjunction.push_back(resultConjunction);
  }
  return resultDisjunction;
}

std::optional<AnnotatedSet> Rules::saturateBase(const AnnotatedSet& annotatedSet) {
  const auto& [set, annotation] = annotatedSet;
  if (annotation->getValue().value_or(INT32_MAX) >= saturationBound) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::setUnion:
      return std::nullopt;  // saturate only inside intersection/domain/image
    case SetOperation::setIntersection: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto rightOperand = std::get<AnnotatedSet>(Annotated::getRight(annotatedSet));
      const auto leftSaturated = saturateBase(leftOperand);
      if (leftSaturated.has_value()) {
        return Annotated::newSet(set->operation, *leftSaturated, rightOperand);
      }
      const auto rightSaturated = saturateBase(rightOperand);
      if (rightSaturated.has_value()) {
        return Annotated::newSet(set->operation, leftOperand, *rightSaturated);
      }
      return std::nullopt;
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto relation = std::get<AnnotatedRelation>(Annotated::getRight(annotatedSet));

      if (set->leftOperand->operation != SetOperation::event) {
        const auto leftSaturated = saturateBase(leftOperand);
        if (leftSaturated.has_value()) {
          return Annotated::newSet(set->operation, *leftSaturated, relation);
        }
        return std::nullopt;
      }

      if (set->relation->operation == RelationOperation::baseRelation) {
        const auto& relationName = *set->relation->identifier;
        if (Assumption::baseAssumptions.contains(relationName)) {
          const auto& assumption = Assumption::baseAssumptions.at(relationName);
          const int newValue = annotation->getRight()->getValue().value_or(0) + 1;
          const auto saturatedRelation = Annotated::makeWithValue(assumption.relation, newValue);
          return Annotated::newSet(set->operation, leftOperand, saturatedRelation);
        }
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<AnnotatedSet> Rules::saturateId(const AnnotatedSet& annotatedSet) {
  const auto& [set, annotation] = annotatedSet;
  if (annotation->getValue().value_or(INT32_MAX) >= saturationBound) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::setUnion:
      return std::nullopt;  // saturate only inside intersection/domain/image
    case SetOperation::setIntersection: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto rightOperand = std::get<AnnotatedSet>(Annotated::getRight(annotatedSet));
      const auto leftSaturated = saturateId(leftOperand);
      if (leftSaturated.has_value()) {
        return Annotated::newSet(set->operation, *leftSaturated, rightOperand);
      }
      const auto rightSaturated = saturateId(rightOperand);
      if (rightSaturated.has_value()) {
        return Annotated::newSet(set->operation, leftOperand, *rightSaturated);
      }
      return std::nullopt;
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto relation = std::get<AnnotatedRelation>(Annotated::getRight(annotatedSet));

      if (set->leftOperand->operation != SetOperation::event) {
        const auto leftSaturated = saturateId(leftOperand);
        if (leftSaturated.has_value()) {
          return Annotated::newSet(set->operation, *leftSaturated, relation);
        }
        return std::nullopt;
      }

      if (set->relation->operation == RelationOperation::baseRelation) {
        // e.b -> (e.R).b
        // TODO: cache this and its annototation
        const auto masterId = Assumption::masterIdRelation();
        const auto saturatedRelation =
            Annotated::makeWithValue(masterId, annotation->getRight()->getValue().value() + 1);
        const auto eR = Annotated::newSet(SetOperation::image, leftOperand, saturatedRelation);
        const auto b = std::get<AnnotatedRelation>(Annotated::getRight(annotatedSet));
        const auto eR_b = Annotated::newSet(set->operation, eR, b);
        return eR_b;
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<DNF> Rules::handleIntersectionWithEvent(const Literal& literal,
                                                      const bool modalRules) {
  assert(literal.set->leftOperand->operation == SetOperation::event ||
         literal.set->rightOperand->operation == SetOperation::event);
  // e & s
  const bool leftRule = literal.set->leftOperand->operation == SetOperation::event;
  CanonicalSet e = leftRule ? literal.set->leftOperand : literal.set->rightOperand;
  CanonicalSet s = leftRule ? literal.set->rightOperand : literal.set->leftOperand;
  CanonicalAnnotation sAnnotation =
      leftRule ? literal.annotation->getRight() : literal.annotation->getLeft();
  AnnotatedSet annotatedS = {s, sAnnotation};

  // LeftRule: handle "e & s != 0"
  // RightRule: handle "s & e != 0"
  assert(e->operation == SetOperation::event);
  // Do case distinction on the shape of "s"
  switch (s->operation) {
    case SetOperation::baseSet:
      // LeftRule: e & A != 0  ->  e \in A
      // RightRule: A & e != 0  ->  e \in A
      return DNF{{Literal(literal.negated, *e->label, *s->identifier)}};
    case SetOperation::event:
      // LeftRule: e & f != 0  ->  e == f
      // RightRule: f & e != 0  ->  e == f (in both cases use same here)
      // Rule (=)
      return DNF{{Literal(literal.negated, *e->label, *s->label)}};
    case SetOperation::emptySet:
      // LeftRule: e & 0 != 0  ->  false
      // RightRule: 0 & e != 0  ->  false
      return literal.negated ? DNF{{TOP}} : DNF{{BOTTOM}};
    case SetOperation::fullSet:
      // LeftRule: e & 1 != 0  ->  true
      // RightRule: 1 & e != 0  ->  true
      return literal.negated ? DNF{{BOTTOM}} : DNF{{TOP}};
    case SetOperation::setIntersection: {
      // LeftRule: e & (s1 & s2) -> e & s1 , e & s2
      // RightRule: (s1 & s2) & e -> s1 & e , s2 & e
      const CanonicalSet e_and_s1 =
          leftRule ? Set::newSet(SetOperation::setIntersection, e, s->leftOperand)
                   : Set::newSet(SetOperation::setIntersection, s->leftOperand, e);
      const CanonicalSet e_and_s2 =
          leftRule ? Set::newSet(SetOperation::setIntersection, e, s->rightOperand)
                   : Set::newSet(SetOperation::setIntersection, s->rightOperand, e);
      // Rule (~&_1L), Rule (&_1L)
      if (!literal.negated) {
        return DNF{{literal.substituteSet(AnnotatedSet(e_and_s1, Annotation::none())),
                    literal.substituteSet(AnnotatedSet(e_and_s2, Annotation::none()))}};
      }

      auto e_and_s1_annotation =
          leftRule ? Annotation::newAnnotation(Annotation::none(), sAnnotation->getLeft())
                   : Annotation::newAnnotation(sAnnotation->getLeft(), Annotation::none());
      auto e_and_s2_annotation =
          leftRule ? Annotation::newAnnotation(Annotation::none(), sAnnotation->getRight())
                   : Annotation::newAnnotation(sAnnotation->getLeft(), Annotation::none());

      return DNF{{literal.substituteSet(AnnotatedSet(e_and_s1, e_and_s1_annotation))},
                 {literal.substituteSet(AnnotatedSet(e_and_s2, e_and_s2_annotation))}};
    }
    case SetOperation::setUnion: {
      return std::nullopt;  // handled in later function
    }
    // -------------- Complex case --------------
    case SetOperation::image:
    case SetOperation::domain: {
      // LeftRule: e & (s'r)     or      e & (rs')
      // RightRule: (s'r) & e    or      (rs') & e
      const CanonicalSet sp = s->leftOperand;
      const CanonicalRelation r = s->relation;
      CanonicalAnnotation spa, ra;
      if (literal.negated) {
        spa = sAnnotation->getLeft();
        ra = sAnnotation->getRight();
      }

      if (sp->operation != SetOperation::event) {
        // LeftRule: e & s'r -> re & s'     or      e & rs' -> er & s'
        // Rule (._12L)     or      Rule (._21L)
        // RightRule: s'r & e -> s' & re        or      rs' & e -> s' & er
        // Rule (._12R)     or      Rule (._21R)
        const SetOperation oppositeOp =
            s->operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
        const CanonicalSet swapped = Set::newSet(oppositeOp, e, r);  // re   or    er
        const CanonicalSet swapped_and_sp =
            leftRule ? Set::newSet(SetOperation::setIntersection, swapped, sp)
                     : Set::newSet(SetOperation::setIntersection, sp, swapped);

        if (!literal.negated) {
          return DNF{{literal.substituteSet(AnnotatedSet(swapped_and_sp, Annotation::none()))}};
        }

        auto swapped_annotation = Annotation::newAnnotation(Annotation::none(), ra);  // re   or er
        auto swapped_and_sp_annotation = leftRule
                                             ? Annotation::newAnnotation(swapped_annotation, spa)
                                             : Annotation::newAnnotation(spa, swapped_annotation);
        return DNF{
            {literal.substituteSet(AnnotatedSet(swapped_and_sp, swapped_and_sp_annotation))}};
      } else if (r->operation == RelationOperation::baseRelation) {
        // LeftRule: e & (f.b)     or      e & (b.f)
        // RightRule: f.b & e      or      b.f & e
        // shortcut multiple rules
        const std::string b = *r->identifier;
        int first = *e->label;
        int second = *sp->label;
        if (s->operation == SetOperation::image) {
          std::swap(first, second);
        }

        // (first, second) \in b
        if (!literal.negated) {
          return DNF{{Literal(first, second, b)}};
        }

        assert(spa->isLeaf());
        const int value = spa->getValue().value_or(0);
        return DNF{{Literal(first, second, b, value)}};
      } else {
        // LeftRule: e & fr     or      e & rf
        // RightRule: fr & e      or      rf & e
        // -> r is not base
        // -> apply some rule to fr    or      rf

        const auto sResult = applyRule(literal, annotatedS, modalRules);
        if (!sResult) {
          // no rule applicable (possible since we omit rules where true is derivable)
          return std::nullopt;
        }

        // LeftRule: e & sResult
        // RightRule: sResult & e
        DNF result;
        result.reserve(sResult->size());
        for (const auto& partialCube : *sResult) {
          Cube cube;
          cube.reserve(partialCube.size());

          for (const auto& partialLiteral : partialCube) {
            if (std::holds_alternative<Literal>(partialLiteral)) {
              auto l = std::get<Literal>(partialLiteral);
              cube.push_back(std::move(l));
            } else {
              const auto asi = std::get<AnnotatedSet>(partialLiteral);
              const auto si = std::get<CanonicalSet>(asi);
              const auto ai = std::get<CanonicalAnnotation>(asi);
              const CanonicalSet e_and_si = leftRule
                                                ? Set::newSet(SetOperation::setIntersection, e, si)
                                                : Set::newSet(SetOperation::setIntersection, si, e);
              auto e_and_si_annotation = leftRule
                                             ? Annotation::newAnnotation(Annotation::none(), ai)
                                             : Annotation::newAnnotation(ai, Annotation::none());
              cube.emplace_back(literal.substituteSet(AnnotatedSet(e_and_si, e_and_si_annotation)));
            }
          }
          result.push_back(cube);
        }
        return result;
      }
    }
    default:
      throw std::logic_error("unreachable");
  }
  throw std::logic_error("unreachable");
}

std::optional<DNF> Rules::applyRule(const Literal& literal, const bool modalRules) {
  switch (literal.operation) {
    case PredicateOperation::edge:
    case PredicateOperation::constant:
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      // (\neg=): ~(e = e) -> FALSE
      if (literal.leftLabel == literal.rightLabel) {
        return literal.negated ? DNF{{BOTTOM}} : DNF{{TOP}};
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::setNonEmptiness:
      if (literal.set->operation == SetOperation::setIntersection) {
        // s1 & s2 != 0
        if (literal.set->leftOperand->operation == SetOperation::event ||
            literal.set->rightOperand->operation == SetOperation::event) {
          return handleIntersectionWithEvent(literal, modalRules);
        }
      }
      // apply non-root rules
      if (const auto result = applyRule(literal, literal.annotatedSet(), modalRules)) {
        return toDNF(literal, *result);
      }
      return std::nullopt;
    default:
      throw std::logic_error("unreachable");
  }
  throw std::logic_error("unreachable");
}

std::optional<Literal> Rules::saturateBase(const Literal& literal) {
  if (!literal.negated) {
    return std::nullopt;  // no rule applicable
  }

  if (literal.annotation->getValue().value_or(INT32_MAX) >= saturationBound) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::edge: {
      assert(literal.annotation->isLeaf());

      auto it = Assumption::baseAssumptions.find(*literal.identifier);
      if (it == Assumption::baseAssumptions.end()) {
        return std::nullopt;
      }

      // (e1, e2) \in b, R <= b -> e1R & e2
      const auto assumption = std::get<Assumption>(*it);
      const CanonicalSet e1 = Set::newEvent(*literal.leftLabel);
      const CanonicalSet e2 = Set::newEvent(*literal.rightLabel);
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, assumption.relation);
      const CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::setIntersection, e1R, e2);
      const AnnotationType newValue = literal.annotation->getValue().value_or(0) + 1;
      // annotation tree should be the on of e1R_and_e2
      return Literal(Annotated::makeWithValue(e1R_and_e2, newValue));
    }
    case PredicateOperation::setNonEmptiness: {
      const auto saturatedLiteral = saturateBase(literal.annotatedSet());
      if (saturatedLiteral.has_value()) {
        return Literal(*saturatedLiteral);
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<Literal> Rules::saturateId(const Literal& literal) {
  if (!literal.negated) {
    return std::nullopt;
  }

  if (literal.annotation->getValue().value_or(INT32_MAX) >= saturationBound &&
      literal.operation != PredicateOperation::equality) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    // negative equality predicates can be saturated but have no value
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::equality: {
      // ~e1 = e2 -> ~e1R & e2
      const CanonicalSet e1 = Set::newEvent(*literal.leftLabel);
      const CanonicalSet e2 = Set::newEvent(*literal.rightLabel);
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      const CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::setIntersection, e1R, e2);
      // annotation tree should be the on of R
      // FIXME: masterId and annotation for masterID should be cached
      return Literal(Annotated::makeWithValue(e1R_and_e2, 1));
    }
    case PredicateOperation::edge: {
      // ~(e1, e2) \in b, R <= id -> ~e1R & b.Re2
      const CanonicalSet e1 = Set::newEvent(*literal.leftLabel);
      const CanonicalSet e2 = Set::newEvent(*literal.rightLabel);
      const CanonicalRelation b = Relation::newBaseRelation(*literal.identifier);
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      const CanonicalSet Re2 =
          Set::newSet(SetOperation::domain, e2, Assumption::masterIdRelation());
      const CanonicalSet b_Re2 = Set::newSet(SetOperation::domain, Re2, b);
      const CanonicalSet e1R_and_bRe2 = Set::newSet(SetOperation::setIntersection, e1R, b_Re2);
      const AnnotationType newValue = literal.annotation->getValue().value_or(0) + 1;

      // annotation tree should be the on of e1R_and_bRe2
      assert(literal.annotation->isLeaf());
      return Literal(Annotated::makeWithValue(e1R_and_bRe2, newValue));
    }
    case PredicateOperation::setNonEmptiness: {
      auto saturatedLiteral = saturateId(literal.annotatedSet());
      if (saturatedLiteral) {
        return Literal(*saturatedLiteral);
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<PartialDNF> Rules::applyRule(const Literal& context, const AnnotatedSet& annotatedSet,
                                           const bool modalRules) {
  const auto& [set, setAnnotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::event:
      // no rule applicable to single event constant
      return std::nullopt;
    case SetOperation::emptySet:
      // Rule (\bot_1):
      return PartialDNF{{BOTTOM}};
    case SetOperation::fullSet: {
      if (context.negated) {
        // Rule (\neg\top_1): this case needs context (handled later)
        return std::nullopt;
      }
      // Rule (\top_1): [T] -> { [f] } , only if positive
      const CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
      return PartialDNF{{AnnotatedSet(f, nullptr)}};
    }
    case SetOperation::setUnion: {
      // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
      // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
      if (!context.negated) {
        return PartialDNF{{AnnotatedSet(set->leftOperand, Annotation::none())},
                          {AnnotatedSet(set->rightOperand, Annotation::none())}};
      }
      return PartialDNF{{AnnotatedSet(set->leftOperand, setAnnotation->getLeft()),
                         AnnotatedSet(set->rightOperand, setAnnotation->getRight())}};
    }
    case SetOperation::setIntersection: {
      if (set->leftOperand->operation != SetOperation::event &&
          set->rightOperand->operation != SetOperation::event) {
        // [S1 & S2]: apply rules recursively
        const auto leftResult = applyRule(context, Annotated::getLeft(annotatedSet), modalRules);
        if (leftResult) {
          const auto& disjunction = *leftResult;
          return substituteIntersectionOperand(
              false, disjunction, AnnotatedSet(set->rightOperand, setAnnotation->getRight()));
        }
        const auto rightResult = applyRule(
            context, AnnotatedSet(set->rightOperand, setAnnotation->getRight()), modalRules);
        if (rightResult) {
          const auto& disjunction = *rightResult;
          return substituteIntersectionOperand(true, disjunction, Annotated::getLeft(annotatedSet));
        }
        return std::nullopt;
      }

      const bool isRoot = context.set == set;  // e & s != 0
      if (isRoot) {
        // do not apply (eL) rule
        throw std::logic_error("unreachable");  // is implemented in handleIntersectionWithEvent //
                                                // TODO maybe move here?
      }
      // Case [e & s] OR [s & e]
      // Rule (~eL) / (~eR):
      // Rule (eL): [e & s] -> { [e], e.s }
      // Rule (eR): [s & e] -> { [e], s.e }
      const auto intersection =
          Annotated::newSet(SetOperation::setIntersection, Annotated::getLeft(annotatedSet),
                            std::get<AnnotatedSet>(Annotated::getRight(annotatedSet)));
      const CanonicalSet& singleton =
          set->leftOperand->operation == SetOperation::event ? set->leftOperand : set->rightOperand;
      const Literal substitute = context.substituteSet(intersection);

      return context.negated
                 ? PartialDNF{{AnnotatedSet(singleton, Annotation::none())}, {substitute}}
                 : PartialDNF{{AnnotatedSet(singleton, Annotation::none()), substitute}};
    }
    case SetOperation::baseSet: {
      if (context.negated) {
        // Rule (~A): requires context (handled later)
        return std::nullopt;
      }
      // Rule (A): [B] -> { [f], f \in B }
      const CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
      return PartialDNF{{AnnotatedSet(f, nullptr), Literal(false, *f->label, *set->identifier)}};
    }
    case SetOperation::image:
    case SetOperation::domain: {
      if (set->leftOperand->operation == SetOperation::event) {
        return applyRelationalRule(context, annotatedSet, modalRules);
      }

      const auto setResult = applyRule(context, Annotated::getLeft(annotatedSet), modalRules);
      if (!setResult) {
        return std::nullopt;
      }

      std::vector<std::vector<PartialLiteral>> result;
      result.reserve(setResult->size());
      for (const auto& cube : *setResult) {
        std::vector<PartialLiteral> newCube;
        newCube.reserve(cube.size());
        for (const auto& partialPredicate : cube) {
          if (std::holds_alternative<Literal>(partialPredicate)) {
            newCube.push_back(partialPredicate);
          } else {
            const auto& s = std::get<AnnotatedSet>(partialPredicate);
            // TODO: there should only be one [] inside each {}
            // otherwise we have to intersect (&) all  []'s after before replacing
            // currently we just assume this is the case without further checking
            const auto annotatedRelation =
                std::get<AnnotatedRelation>(Annotated::getRight(annotatedSet));
            const auto newSet = Annotated::newSet(set->operation, s, annotatedRelation);
            newCube.emplace_back(newSet);
          }
        }
        result.push_back(newCube);
      }
      return result;
    }
    default:
      throw std::logic_error("unreachable");
  }
}
