#include "Rules.h"

#include <spdlog/spdlog.h>

#include "../utility.h"

std::optional<PartialDNF> Rules::applyRelationalRule(const Literal& context,
                                                     const SaturationAnnotatedSet& annotatedSet

) {
  // e.r
  const CanonicalSet set = std::get<CanonicalSet>(annotatedSet);
  const auto setAnnotation = std::get<CanonicalAnnotation<SaturationAnnotation>>(annotatedSet);
  const CanonicalSet event = set->leftOperand;
  const CanonicalRelation relation = set->relation;
  const auto relationAnnotation = setAnnotation->getRight();
  const SetOperation operation = set->operation;
  // operation indicates if left or right rule is used
  // in this function all variable names reflect the case for SetOperation::image
  assert(operation == SetOperation::image || operation == SetOperation::domain);

  switch (relation->operation) {
    case RelationOperation::baseRelation: {
      if (context.negated) {
        // Rule (~aL), Rule (~aR): requires context (handled later)
        return std::nullopt;
      }

      // positive modal rule are handled in other method
      return std::nullopt;
    }
    case RelationOperation::setIdentity: {
      // Rule could be handled by cartesianProducts using [S] == SxS & id
      // We use more direct Rule: [e[S]] -> { e & S, [e] }
      //  ~[e[S]] -> { ~e & S } , { ~[e] }
      CanonicalSet eAndS = Set::newSet(SetOperation::setIntersection, event, relation->set);

      if (!context.negated) {
        return PartialDNF{{Literal(eAndS), SaturationAnnotatedSet(
                                               event, Annotation<SaturationAnnotation>::none())}};
      }

      return PartialDNF{{Literal(SaturationAnnotatedSet(eAndS, setAnnotation))},
                        {SaturationAnnotatedSet(event, Annotation<SaturationAnnotation>::none())}};
    }
    case RelationOperation::cartesianProduct: {
      // TODO: implement
      spdlog::error("[Error] Cartesian products are currently not supported.");
      assert(false);
    }
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
        return PartialDNF{{SaturationAnnotatedSet(er1, Annotation<SaturationAnnotation>::none())},
                          {SaturationAnnotatedSet(er2, Annotation<SaturationAnnotation>::none())}};
      }

      auto er1_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation->getLeft());
      auto er2_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation->getRight());
      return PartialDNF{{SaturationAnnotatedSet(er1, er1_annotation),
                         SaturationAnnotatedSet(er2, er2_annotation)}};
    }
    case RelationOperation::composition: {
      // Rule (._22L): [e(a.b)] -> { [(e.a)b] }
      // Rule (._22R): [(b.a)e] -> { [b(a.e)] }
      const auto a =
          operation == SetOperation::image ? relation->leftOperand : relation->rightOperand;
      const auto b =
          operation == SetOperation::image ? relation->rightOperand : relation->leftOperand;
      const CanonicalSet ea = Set::newSet(operation, event, a);
      const CanonicalSet ea_b = Set::newSet(operation, ea, b);

      if (!context.negated) {
        return PartialDNF{{SaturationAnnotatedSet(ea_b, Annotation<SaturationAnnotation>::none())}};
      }

      const auto a_annotation = operation == SetOperation::image ? relationAnnotation->getLeft()
                                                                 : relationAnnotation->getRight();
      const auto b_annotation = operation == SetOperation::image ? relationAnnotation->getRight()
                                                                 : relationAnnotation->getLeft();
      const auto ea_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), a_annotation);
      const auto ea_b_annotation =
          Annotation<SaturationAnnotation>::meetAnnotation(ea_annotation, b_annotation);
      return PartialDNF{{SaturationAnnotatedSet(ea_b, ea_b_annotation)}};
    }
    case RelationOperation::converse: {
      // Rule (^-1L): [e.(r^-1)] -> { [r.e] }
      const SetOperation oppositOp =
          operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
      CanonicalSet re = Set::newSet(oppositOp, event, relation->leftOperand);

      if (!context.negated) {
        return PartialDNF{{SaturationAnnotatedSet(re, Annotation<SaturationAnnotation>::none())}};
      }

      auto re_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation);
      return PartialDNF{{SaturationAnnotatedSet(re, re_annotation)}};
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
      return PartialDNF{{SaturationAnnotatedSet(event, Annotation<SaturationAnnotation>::none())}};
    }
    case RelationOperation::relationIntersection: {
      // Rule (\cap_2\lrule): [e.(r1 & r2)] -> { [e.r1 & e.r2] }
      const CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      const CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      const CanonicalSet er1_and_er2 = Set::newSet(SetOperation::setIntersection, er1, er2);

      if (!context.negated) {
        return PartialDNF{
            {SaturationAnnotatedSet(er1_and_er2, Annotation<SaturationAnnotation>::none())}};
      }

      const auto er1_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation->getLeft());
      const auto er2_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation->getRight());
      const auto er1_and_er2_annotation =
          Annotation<SaturationAnnotation>::meetAnnotation(er1_annotation, er2_annotation);
      return PartialDNF{{SaturationAnnotatedSet(er1_and_er2, er1_and_er2_annotation)}};
    }
    case RelationOperation::transitiveClosure: {
      lastRuleWasUnrolling = true;
      // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
      // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
      const CanonicalSet er = Set::newSet(operation, event, relation->leftOperand);
      const CanonicalSet err_star = Set::newSet(operation, er, relation);

      if (!context.negated) {
        return PartialDNF{
            {SaturationAnnotatedSet(err_star, Annotation<SaturationAnnotation>::none())},
            {SaturationAnnotatedSet(event, Annotation<SaturationAnnotation>::none())}};
      }

      const auto er_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
          Annotation<SaturationAnnotation>::none(), relationAnnotation);
      const auto err_star_annotation =
          Annotation<SaturationAnnotation>::meetAnnotation(er_annotation, relationAnnotation);
      return PartialDNF{{SaturationAnnotatedSet(err_star, err_star_annotation),
                         SaturationAnnotatedSet(event, Annotation<SaturationAnnotation>::none())}};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

PartialDNF Rules::substituteIntersectionOperand(const bool substituteRight,
                                                const PartialDNF& disjunction,
                                                const SaturationAnnotatedSet& otherOperand) {
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
        const auto& [s, a] = std::get<SaturationAnnotatedSet>(literal);
        const auto& [os, oa] = otherOperand;
        // substitute
        const auto& newAs =
            substituteRight
                ? SaturationAnnotatedSet{Set::newSet(SetOperation::setIntersection, os, s),
                                         Annotation<SaturationAnnotation>::meetAnnotation(oa, a)}
                : SaturationAnnotatedSet{Set::newSet(SetOperation::setIntersection, s, os),
                                         Annotation<SaturationAnnotation>::meetAnnotation(a, oa)};
        resultConjunction.emplace_back(newAs);
      }
    }
    resultDisjunction.push_back(resultConjunction);
  }
  return resultDisjunction;
}

DNF eventIntersectionWithPartialDNF(const bool isLeftRule, const Literal& context,
                                    const CanonicalSet event, const PartialDNF& partialDnf) {
  DNF result;
  result.reserve(partialDnf.size());
  for (const auto& partialCube : partialDnf) {
    Cube cube;
    cube.reserve(partialCube.size());

    for (const auto& partialLiteral : partialCube) {
      if (std::holds_alternative<Literal>(partialLiteral)) {
        auto l = std::get<Literal>(partialLiteral);
        cube.push_back(std::move(l));
      } else {
        const auto asi = std::get<SaturationAnnotatedSet>(partialLiteral);
        const auto si = std::get<CanonicalSet>(asi);
        const auto ai = std::get<CanonicalAnnotation<SaturationAnnotation>>(asi);
        const CanonicalSet e_and_si = isLeftRule
                                          ? Set::newSet(SetOperation::setIntersection, event, si)
                                          : Set::newSet(SetOperation::setIntersection, si, event);
        auto e_and_si_annotation = isLeftRule ? Annotation<SaturationAnnotation>::meetAnnotation(
                                                    Annotation<SaturationAnnotation>::none(), ai)
                                              : Annotation<SaturationAnnotation>::meetAnnotation(
                                                    ai, Annotation<SaturationAnnotation>::none());
        cube.emplace_back(
            context.substituteSet(SaturationAnnotatedSet(e_and_si, e_and_si_annotation)));
      }
    }
    result.push_back(cube);
  }
  return result;
}

std::optional<SaturationAnnotatedSet> Rules::saturateBase(
    const SaturationAnnotatedSet& annotatedSet) {
  const auto& [set, annotation] = annotatedSet;
  if (!annotation->getValue().has_value() || annotation->getValue().value().second <= 0) {
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
      const auto rightOperand = Annotated::getRightSet<SaturationAnnotation>(annotatedSet);

      const auto leftSaturated = saturateBase(leftOperand);
      const auto rightSaturated = saturateBase(rightOperand);

      if (!leftSaturated && !rightSaturated) {
        return std::nullopt;
      }

      const auto& newLeft = leftSaturated ? leftSaturated.value() : leftOperand;
      const auto& newRight = rightSaturated ? rightSaturated.value() : rightOperand;
      return Annotated::newSet(set->operation, newLeft, newRight);
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto relation = Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);

      if (set->leftOperand->operation != SetOperation::event) {
        if (const auto leftSaturated = saturateBase(leftOperand)) {
          return Annotated::newSet(set->operation, *leftSaturated, relation);
        }
        return std::nullopt;
      }

      if (set->relation->operation == RelationOperation::baseRelation) {
        const auto& relationName = *set->relation->identifier;
        if (Assumption::baseAssumptions.contains(relationName)) {
          const auto& assumption = Assumption::baseAssumptions.at(relationName);
          const auto [idAnn, baseAnn] = annotation->getRight()->getValue().value();
          const auto saturatedRelation = AnnotatedRelation<SaturationAnnotation>{
              assumption.relation,
              Annotated::makeWithValue(assumption.relation, {idAnn, baseAnn - 1})};
          return Annotated::newSet(set->operation, leftOperand, saturatedRelation);
        }
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<SaturationAnnotatedSet> Rules::saturateBaseSet(
    const SaturationAnnotatedSet& annotatedSet) {
  const auto& [set, annotation] = annotatedSet;
  if (!annotation->getValue().has_value() || annotation->getValue().value().second <= 0) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (set->operation) {
    case SetOperation::baseSet: {
      const auto& setName = set->identifier.value();
      if (Assumption::baseSetAssumptions.contains(setName)) {
        const auto& assumption = Assumption::baseSetAssumptions.at(setName);
        const auto [idAnn, baseAnn] = annotation->getValue().value();
        return AnnotatedSet<SaturationAnnotation>{
            assumption.set, Annotated::makeWithValue(assumption.set, {idAnn, baseAnn - 1})};
      }
      return std::nullopt;
    }
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::setUnion:
      return std::nullopt;  // saturate only inside intersection/domain/image
    case SetOperation::setIntersection: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto rightOperand = Annotated::getRightSet<SaturationAnnotation>(annotatedSet);

      const auto leftSaturated = saturateBaseSet(leftOperand);
      const auto rightSaturated = saturateBaseSet(rightOperand);

      if (!leftSaturated && !rightSaturated) {
        return std::nullopt;
      }

      const auto& newLeft = leftSaturated ? leftSaturated.value() : leftOperand;
      const auto& newRight = rightSaturated ? rightSaturated.value() : rightOperand;
      return Annotated::newSet(set->operation, newLeft, newRight);
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto leftOperand = Annotated::getLeft(annotatedSet);
      const auto relation = Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);

      if (const auto leftSaturated = saturateBaseSet(leftOperand)) {
        return Annotated::newSet(set->operation, *leftSaturated, relation);
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<SaturationAnnotatedSet> Rules::saturateId(
    const SaturationAnnotatedSet& annotatedSet) {
  const auto& [set, annotation] = annotatedSet;
  if (!annotation->getValue().has_value() || annotation->getValue().value().first <= 0) {
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
      const auto rightOperand = Annotated::getRightSet<SaturationAnnotation>(annotatedSet);
      if (const auto leftSaturated = saturateId(leftOperand)) {
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
      const auto relation = Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);

      if (set->leftOperand->operation != SetOperation::event) {
        if (const auto leftSaturated = saturateId(leftOperand)) {
          return Annotated::newSet(set->operation, *leftSaturated, relation);
        }
        return std::nullopt;
      }

      if (set->relation->operation == RelationOperation::baseRelation) {
        // e.b -> (e.R).b
        // TODO: cache this and its annotation
        const auto masterId = Assumption::masterIdRelation();
        const auto [idAnn, baseAnn] = annotation->getRight()->getValue().value();
        const auto saturatedRelation = AnnotatedRelation<SaturationAnnotation>{
            masterId, Annotated::makeWithValue(masterId, {0, 0})};
        const auto eR = Annotated::newSet(SetOperation::image, leftOperand, saturatedRelation);
        const auto b = Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);
        const auto eR_b = Annotated::newSet(set->operation, eR, b);
        return eR_b;
      }
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<DNF> Rules::handleIntersectionWithEvent(const Literal& literal) {
  assert(literal.set->leftOperand->isEvent() || literal.set->rightOperand->isEvent());
  // e & s
  const bool leftRule = literal.set->leftOperand->isEvent();
  const CanonicalSet e = leftRule ? literal.set->leftOperand : literal.set->rightOperand;
  const CanonicalSet s = leftRule ? literal.set->rightOperand : literal.set->leftOperand;
  const auto sAnnotation =
      leftRule ? literal.annotation->getRight() : literal.annotation->getLeft();
  const SaturationAnnotatedSet annotatedS = {s, sAnnotation};

  // LeftRule: handle "e & s != 0"
  // RightRule: handle "s & e != 0"
  assert(e->isEvent());
  // Do case distinction on the shape of "s"
  switch (s->operation) {
    case SetOperation::baseSet:
      // LeftRule: e & A != 0  ->  e \in A
      // RightRule: A & e != 0  ->  e \in A
      if (!literal.negated) {
        return DNF{{Literal(e, *s->identifier)}};
      }

      return DNF{{Literal(e, *s->identifier, sAnnotation->getValue().value())}};
    case SetOperation::event:
      // LeftRule: e & f != 0  ->  e == f
      // RightRule: f & e != 0  ->  e == f (in both cases use same here)
      // Rule (=)
      return DNF{{Literal(literal.negated, e, s)}};
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
        return DNF{{literal.substituteSet(
                        SaturationAnnotatedSet(e_and_s1, Annotation<SaturationAnnotation>::none())),
                    literal.substituteSet(SaturationAnnotatedSet(
                        e_and_s2, Annotation<SaturationAnnotation>::none()))}};
      }

      auto e_and_s1_annotation =
          leftRule ? Annotation<SaturationAnnotation>::meetAnnotation(
                         Annotation<SaturationAnnotation>::none(), sAnnotation->getLeft())
                   : Annotation<SaturationAnnotation>::meetAnnotation(
                         sAnnotation->getLeft(), Annotation<SaturationAnnotation>::none());
      auto e_and_s2_annotation =
          leftRule ? Annotation<SaturationAnnotation>::meetAnnotation(
                         Annotation<SaturationAnnotation>::none(), sAnnotation->getRight())
                   : Annotation<SaturationAnnotation>::meetAnnotation(
                         sAnnotation->getRight(), Annotation<SaturationAnnotation>::none());

      return DNF{{literal.substituteSet(SaturationAnnotatedSet(e_and_s1, e_and_s1_annotation))},
                 {literal.substituteSet(SaturationAnnotatedSet(e_and_s2, e_and_s2_annotation))}};
    }
    case SetOperation::setUnion: {
      const auto sResult = applyRule(literal, annotatedS);
      assert(sResult);
      // LeftRule: e & sResult
      // RightRule: sResult & e
      return eventIntersectionWithPartialDNF(leftRule, literal, e, sResult.value());
    }
    // -------------- Complex case --------------
    case SetOperation::image:
    case SetOperation::domain: {
      // LeftRule: e & (s'r)     or      e & (rs')
      // RightRule: (s'r) & e    or      (rs') & e
      const CanonicalSet sp = s->leftOperand;
      const CanonicalRelation r = s->relation;

      if (!sp->isEvent()) {
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
          return DNF{{literal.substituteSet(
              SaturationAnnotatedSet(swapped_and_sp, Annotation<SaturationAnnotation>::none()))}};
        }

        const auto spa = sAnnotation->getLeft();
        const auto ra = sAnnotation->getRight();
        const auto swapped_annotation = Annotation<SaturationAnnotation>::meetAnnotation(
            Annotation<SaturationAnnotation>::none(), ra);  // re   or er
        const auto swapped_and_sp_annotation =
            leftRule ? Annotation<SaturationAnnotation>::meetAnnotation(swapped_annotation, spa)
                     : Annotation<SaturationAnnotation>::meetAnnotation(spa, swapped_annotation);
        return DNF{{literal.substituteSet(
            SaturationAnnotatedSet(swapped_and_sp, swapped_and_sp_annotation))}};
      }

      if (r->operation == RelationOperation::baseRelation) {
        // LeftRule: e & (f.b)     or      e & (b.f)
        // RightRule: f.b & e      or      b.f & e
        // shortcut multiple rules
        assert(e->isEvent());
        assert(sp->isEvent());
        const std::string b = *r->identifier;
        auto first = e;
        auto second = sp;
        if (s->operation == SetOperation::image) {
          std::swap(first, second);
        }

        // (first, second) \in b
        if (!literal.negated) {
          return DNF{{Literal(first, second, b)}};
        }

        const auto ra = sAnnotation->getRight();
        assert(ra->isLeaf() && ra->getValue().has_value());
        const SaturationAnnotation value = ra->getValue().value();
        return DNF{{Literal(first, second, b, value)}};
      }

      // LeftRule: e & fr     or      e & rf
      // RightRule: fr & e      or      rf & e
      // -> r is not base
      // -> apply some rule to fr    or      rf

      const auto sResult = applyRule(literal, annotatedS);
      if (!sResult) {
        // no rule applicable (possible since we omit rules where true is derivable)
        return std::nullopt;
      }

      // LeftRule: e & sResult
      // RightRule: sResult & e
      return eventIntersectionWithPartialDNF(leftRule, literal, e, sResult.value());
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<DNF> Rules::applyRule(const Literal& literal) {
  lastRuleWasUnrolling = false;
  switch (literal.operation) {
    case PredicateOperation::edge:
    case PredicateOperation::constant:
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      // (\neg=): ~(e = e) -> FALSE
      if (literal.leftEvent == literal.rightEvent) {
        return literal.negated ? DNF{{BOTTOM}} : DNF{{TOP}};
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::setNonEmptiness:
      if (literal.set->operation == SetOperation::setIntersection &&
          (literal.set->leftOperand->isEvent() || literal.set->rightOperand->isEvent())) {
        // e & s != 0
        return handleIntersectionWithEvent(literal);
      }
      // apply non-root rules
      if (const auto result = applyRule(literal, literal.annotatedSet())) {
        return toDNF(literal, *result);
      }
      return std::nullopt;
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<DNF> Rules::applyPositiveModalRule(const Literal& literal, const int minimalEvent) {
  if (literal.negated) {
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::edge:
    case PredicateOperation::constant:
    case PredicateOperation::set:
    case PredicateOperation::equality:
      return std::nullopt;  // no rule applicable
    case PredicateOperation::setNonEmptiness:
      if (const auto result = applyPositiveModalRule(literal.annotatedSet(), minimalEvent)) {
        return toDNF(literal, *result);
      }
      return std::nullopt;
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<Literal> Rules::saturateBase(const Literal& literal) {
  if (!literal.negated) {
    return std::nullopt;  // no rule applicable
  }

  if (!literal.annotation->getValue().has_value() ||
      literal.annotation->getValue().value().second <= 0) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::edge: {
      assert(literal.annotation->isLeaf());

      const auto it = Assumption::baseAssumptions.find(*literal.identifier);
      if (it == Assumption::baseAssumptions.end()) {
        return std::nullopt;
      }

      // (e1, e2) \in b, R <= b -> e1R & e2
      const auto assumption = std::get<Assumption>(*it);
      const CanonicalSet e1 = literal.leftEvent;
      const CanonicalSet e2 = literal.rightEvent;
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, assumption.relation);
      const CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::setIntersection, e1R, e2);
      assert(literal.annotation->isLeaf() && literal.annotation->getValue().has_value());
      const auto [idAnn, baseAnn] = literal.annotation->getValue().value();
      // annotation tree should be the on of e1R_and_e2
      return Literal(AnnotatedSet<SaturationAnnotation>{
          e1R_and_e2, Annotated::makeWithValue(e1R_and_e2, {idAnn, baseAnn - 1})});
    }
    case PredicateOperation::setNonEmptiness: {
      if (const auto saturatedLiteral = saturateBase(literal.annotatedSet())) {
        return Literal(saturatedLiteral.value());
      }
      return std::nullopt;
    }
    case PredicateOperation::set:
      return std::nullopt;  // handled in saturateBaseSet
    default:
      throw std::logic_error("unreachable");
  }
}

// TODO: refactor merge with saturateBase?
std::optional<Literal> Rules::saturateBaseSet(const Literal& literal) {
  if (!literal.negated) {
    return std::nullopt;  // no rule applicable
  }

  if (!literal.annotation->getValue().has_value() ||
      literal.annotation->getValue().value().second <= 0) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::set: {
      assert(literal.annotation->isLeaf());
      // e \in B, s <= B -> e & s
      const auto e = literal.leftEvent;
      const auto B = literal.identifier.value();

      if (!Assumption::baseSetAssumptions.contains(B)) {
        return std::nullopt;
      }

      const auto assumption = Assumption::baseSetAssumptions.at(B);
      const auto e_and_s = Set::newSet(SetOperation::setIntersection, e, assumption.set);
      assert(literal.annotation->isLeaf() && literal.annotation->getValue().has_value());
      const auto [idAnn, baseAnn] = literal.annotation->getValue().value();
      return Literal(AnnotatedSet<SaturationAnnotation>{
          e_and_s, Annotated::makeWithValue(e_and_s, {idAnn, baseAnn - 1})});
    }
    case PredicateOperation::setNonEmptiness: {
      if (const auto saturatedLiteral = saturateBaseSet(literal.annotatedSet())) {
        return Literal(saturatedLiteral.value());
      }
      return std::nullopt;
    }
    case PredicateOperation::edge:
      return std::nullopt;  // handled in saturateBase
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<Literal> Rules::saturateId(const Literal& literal) {
  if (!literal.negated) {
    return std::nullopt;
  }

  if ((!literal.annotation->getValue().has_value() ||
       literal.annotation->getValue().value().first <= 0) &&
      literal.operation != PredicateOperation::equality) {
    // We reached the saturation bound everywhere, or there is nothing to saturate
    // negative equality predicates can be saturated but have no value
    return std::nullopt;
  }

  switch (literal.operation) {
    case PredicateOperation::equality: {
      // ~e1 = e2 -> ~e1R & e2
      const CanonicalSet e1 = literal.leftEvent;
      const CanonicalSet e2 = literal.rightEvent;
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      const CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::setIntersection, e1R, e2);
      // annotation tree should be the on of R
      // FIXME: masterId and annotation for masterID should be cached
      return Literal(AnnotatedSet<SaturationAnnotation>{
          e1R_and_e2, Annotated::makeWithValue(e1R_and_e2, {0, 0})});
    }
    case PredicateOperation::edge: {
      // ~(e1, e2) \in b, R <= id -> ~e1R & b.Re2
      const CanonicalSet e1 = literal.leftEvent;
      const CanonicalSet e2 = literal.rightEvent;
      const CanonicalRelation b = Relation::newBaseRelation(*literal.identifier);
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      const CanonicalSet Re2 =
          Set::newSet(SetOperation::domain, e2, Assumption::masterIdRelation());
      const CanonicalSet b_Re2 = Set::newSet(SetOperation::domain, Re2, b);
      const CanonicalSet e1R_and_bRe2 = Set::newSet(SetOperation::setIntersection, e1R, b_Re2);
      assert(literal.annotation->isLeaf() && literal.annotation->getValue().has_value());
      const auto [idAnn, baseAnn] = literal.annotation->getValue().value();

      // annotation tree should be the on of e1R_and_bRe2
      assert(literal.annotation->isLeaf());
      return Literal(AnnotatedSet<SaturationAnnotation>{
          e1R_and_bRe2, Annotated::makeWithValue(e1R_and_bRe2, {0, 0})});
    }
    case PredicateOperation::setNonEmptiness: {
      if (const auto saturatedLiteral = saturateId(literal.annotatedSet())) {
        return Literal(*saturatedLiteral);
      }
      return std::nullopt;
    }
    case PredicateOperation::set: {
      return std::nullopt;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<PartialDNF> Rules::applyRule(const Literal& context,
                                           const SaturationAnnotatedSet& annotatedSet) {
  const auto& [set, setAnnotation] = annotatedSet;
  switch (set->operation) {
    // case SetOperation::topEvent:
    case SetOperation::event:
      // no rule applicable to single event constant
      return context.negated ? PartialDNF{{BOTTOM}} : PartialDNF{{TOP}};
    case SetOperation::emptySet:
      // Rule (\bot_1):
      return PartialDNF{{BOTTOM}};
    case SetOperation::fullSet: {
      if (context.negated) {
        return std::nullopt;
      }
      // Rule (\top_1): [T] -> { [f] } , only if positive
      const CanonicalSet f = Set::freshEvent();
      return PartialDNF{{SaturationAnnotatedSet(f, nullptr)}};
    }
    case SetOperation::setUnion: {
      // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
      // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
      if (!context.negated) {
        return PartialDNF{
            {SaturationAnnotatedSet(set->leftOperand, Annotation<SaturationAnnotation>::none())},
            {SaturationAnnotatedSet(set->rightOperand, Annotation<SaturationAnnotation>::none())}};
      }
      return PartialDNF{{Annotated::getLeft(annotatedSet),
                         Annotated::getRightSet<SaturationAnnotation>(annotatedSet)}};
    }
    case SetOperation::setIntersection: {
      if (!set->leftOperand->isEvent() && !set->rightOperand->isEvent()) {
        // [S1 & S2]: apply rules recursively
        if (const auto leftResult = applyRule(context, Annotated::getLeft(annotatedSet))) {
          const auto& dnf = *leftResult;
          return substituteIntersectionOperand(
              false, dnf, Annotated::getRightSet<SaturationAnnotation>(annotatedSet));
        }
        if (const auto rightResult =
                applyRule(context, Annotated::getRightSet<SaturationAnnotation>(annotatedSet))) {
          const auto& dnf = *rightResult;
          return substituteIntersectionOperand(true, dnf, Annotated::getLeft(annotatedSet));
        }
        return std::nullopt;
      }

      const bool isRoot = context.set == set;  // e & s != 0
      if (isRoot) {
        // do not apply (eL) rule
        throw std::logic_error("unreachable");
        // case is implemented in handleIntersectionWithEvent TODO: maybe move here?
      }
      // Case [e & s] OR [s & e]
      // Rule (~eL) / (~eR):
      // Rule (eL): [e & s] -> { [e], e.s }
      // Rule (eR): [s & e] -> { [e], s.e }
      const auto intersection =
          Annotated::newSet(SetOperation::setIntersection, Annotated::getLeft(annotatedSet),
                            Annotated::getRightSet<SaturationAnnotation>(annotatedSet));
      const CanonicalSet& e = set->leftOperand->isEvent() ? set->leftOperand : set->rightOperand;
      const Literal substitute = context.substituteSet(intersection);

      return context.negated
                 ? PartialDNF{{SaturationAnnotatedSet(e, Annotation<SaturationAnnotation>::none())},
                              {substitute}}
                 : PartialDNF{{SaturationAnnotatedSet(e, Annotation<SaturationAnnotation>::none()),
                               substitute}};
    }
    case SetOperation::baseSet: {
      if (context.negated) {
        // Rule (~A): requires context (handled later)
        return std::nullopt;
      }
      // Rule (A): [B] -> { [f], f \in B }
      const CanonicalSet f = Set::freshEvent();
      return PartialDNF{{SaturationAnnotatedSet(f, Annotation<SaturationAnnotation>::none()),
                         Literal(f, *set->identifier)}};
    }
    case SetOperation::image:
    case SetOperation::domain: {
      if (set->leftOperand->isEvent()) {
        return applyRelationalRule(context, annotatedSet);
      }

      const auto setResult = applyRule(context, Annotated::getLeft(annotatedSet));
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
            const auto& s = std::get<SaturationAnnotatedSet>(partialPredicate);
            // TODO: there should only be one [] inside each {}
            // otherwise we have to intersect (&) all  []'s after before replacing
            // currently we just assume this is the case without further checking
            const auto annotatedRelation =
                Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);
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

std::optional<PartialDNF> Rules::applyPositiveModalRule(const SaturationAnnotatedSet& annotatedSet,
                                                        const int minimalEvent) {
  const auto& [set, setAnnotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::setUnion:
    case SetOperation::baseSet:
      return std::nullopt;  // no rule applicable
    case SetOperation::setIntersection: {
      // [S1 & S2]: apply rules recursively
      if (const auto leftResult =
              applyPositiveModalRule(Annotated::getLeft(annotatedSet), minimalEvent)) {
        return substituteIntersectionOperand(
            false, leftResult.value(), Annotated::getRightSet<SaturationAnnotation>(annotatedSet));
      }
      if (const auto rightResult = applyPositiveModalRule(
              Annotated::getRightSet<SaturationAnnotation>(annotatedSet), minimalEvent)) {
        return substituteIntersectionOperand(true, rightResult.value(),
                                             Annotated::getLeft(annotatedSet));
      }
      return std::nullopt;
    }
    case SetOperation::image:
    case SetOperation::domain: {
      if (set->leftOperand->isEvent() && set->leftOperand->label.value() == minimalEvent) {
        if (set->relation->operation != RelationOperation::baseRelation) {
          return std::nullopt;
        }
        // LeftRule: [e.b] -> { [f], (e,f) \in b }
        // -> Rule (aL)
        // RightRule: [b.e] -> { [f], (f,e) \in b }
        // -> Rule (aR)
        const CanonicalSet e = set->leftOperand;
        const CanonicalSet f = Set::freshEvent();
        const auto& b = *set->relation->identifier;
        const CanonicalSet first = set->operation == SetOperation::image ? e : f;
        const CanonicalSet second = set->operation == SetOperation::image ? f : e;

        return PartialDNF{{SaturationAnnotatedSet(f, Annotation<SaturationAnnotation>::none()),
                           Literal(first, second, b)}};
      }

      const auto setResult = applyPositiveModalRule(Annotated::getLeft(annotatedSet), minimalEvent);
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
            const auto& s = std::get<SaturationAnnotatedSet>(partialPredicate);
            // TODO: there should only be one [] inside each {}
            // otherwise we have to intersect (&) all  []'s after before replacing
            // currently we just assume this is the case without further checking
            const auto annotatedRelation =
                Annotated::getRightRelation<SaturationAnnotation>(annotatedSet);
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
