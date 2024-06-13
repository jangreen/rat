#include "LogicVisitor.h"

#include "../Assumption.h"
#include "../RegularTableau.h"

/*std::vector<Formula>*/ std::any Logic::visitProof(LogicParser::ProofContext *context) {
  std::vector<Formula> formulas;

  for (auto statementContext : context->statement()) {
    if (statementContext->letDefinition()) {
      visitLetDefinition(statementContext->letDefinition());
    } else if (statementContext->inclusion()) {
      visitInclusion(statementContext->inclusion());
    } else if (statementContext->hypothesis()) {
      visitHypothesis(statementContext->hypothesis());
    } else if (statementContext->assertion()) {
      auto untypedFormula = visitAssertion(statementContext->assertion());
      auto formula = std::any_cast<Formula>(untypedFormula);
      formulas.push_back(std::move(formula));
    }
  }

  // process assumptions
  // emptiness r = 0 |- r1 <= r2 iff |- r1 <= r2 + T.r.T
  for (auto &formula : formulas) {
    for (const auto &assumption : RegularTableau::emptinessAssumptions) {
      Set s(SetOperation::domain, Set(SetOperation::full), Relation(assumption.relation));
      Predicate p(PredicateOperation::intersectionNonEmptiness, Set(SetOperation::full),
                  std::move(s));
      Formula f(FormulaOperation::literal, Literal(true, std::move(p)));

      formula = Formula(FormulaOperation::logicalAnd, std::move(formula), std::move(f));
    }
  }

  return formulas;
}
/*void*/ std::any Logic::visitInclusion(LogicParser::InclusionContext *context) {
  auto _ = Logic::parseMemoryModel(context->FILEPATH()->getText());

  return 0;
}
/*Formula*/ std::any Logic::visitAssertion(LogicParser::AssertionContext *context) {
  if (context->INEQUAL()) {
    // currently only support relations on each side of inequation, otherwise behavuour is
    // undefined
    Relation lhs(context->e1->getText());
    Relation rhs(context->e2->getText());
    Set start(SetOperation::singleton, Set::maxSingletonLabel++);
    Set end(SetOperation::singleton, Set::maxSingletonLabel++);
    Set lImage(SetOperation::image, Set(start), std::move(lhs));
    Set rImage(SetOperation::image, std::move(start), std::move(rhs));

    Predicate lp(PredicateOperation::intersectionNonEmptiness, std::move(lImage), Set(end));
    Predicate rp(PredicateOperation::intersectionNonEmptiness, std::move(rImage), std::move(end));

    Literal ll(false, std::move(lp));
    Literal rl(true, std::move(rp));
    Formula lf(FormulaOperation::literal, std::move(ll));
    Formula rf(FormulaOperation::literal, std::move(rl));
    return Formula(FormulaOperation::logicalAnd, std::move(lf), std::move(rf));
  }
  return Formula(context->f1->getText());
}
/*Formula*/ std::any Logic::visitFormula(LogicParser::FormulaContext *context) {
  // TODO: split up by giving names to different functions inside Logic.g4
  if (context->NOT()) {
    auto f = std::any_cast<Formula>(this->visitFormula(context->f1));
    Formula not_f(FormulaOperation::negation, std::move(f));
    return not_f;
  } else if (context->AMP()) {
    auto f1 = std::any_cast<Formula>(this->visitFormula(context->f1));
    auto f2 = std::any_cast<Formula>(this->visitFormula(context->f2));
    Formula f1_and_f2(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
    return f1_and_f2;
  } else if (context->BAR()) {
    auto f1 = std::any_cast<Formula>(this->visitFormula(context->f1));
    auto f2 = std::any_cast<Formula>(this->visitFormula(context->f2));
    Formula f1_or_f2(FormulaOperation::logicalOr, std::move(f1), std::move(f2));
    return f1_or_f2;
  } else if (context->LPAR() && context->RPAR()) {
    auto f1 = std::any_cast<Formula>(this->visitFormula(context->f1));
    return f1;
  } else {
    auto p = std::any_cast<Predicate>(this->visitPredicate(context->p1));
    Literal l(false, std::move(p));
    return Formula(FormulaOperation::literal, std::move(l));
  }
}
/*Predicate*/ std::any Logic::visitPredicate(LogicParser::PredicateContext *context) {
  std::variant<Set, Relation> e1 =
      std::any_cast<std::variant<Set, Relation>>(this->Logic::visit(context->s1));
  std::variant<Set, Relation> e2 =
      std::any_cast<std::variant<Set, Relation>>(this->Logic::visit(context->s2));
  if (std::holds_alternative<Set>(e1) && std::holds_alternative<Set>(e2)) {
    Set s1 = std::get<Set>(e1);
    Set s2 = std::get<Set>(e2);
    return Predicate(PredicateOperation::intersectionNonEmptiness, std::move(s1), std::move(s2));
  }
  spdlog::error(
      "[Parser] Type mismatch of two operands of the intersectionNonEmptiness predicate.");
  exit(0);
}

/*void*/ std::any Logic::visitHypothesis(LogicParser::HypothesisContext *ctx) {
  Relation lhs(ctx->lhs->getText());
  Relation rhs(ctx->rhs->getText());
  switch (rhs.operation) {
    case RelationOperation::base: {
      Assumption assumption(AssumptionType::regular, std::move(lhs), *rhs.identifier);
      RegularTableau::baseAssumptions.insert({*assumption.baseRelation, std::move(assumption)});
      return 0;
    }
    case RelationOperation::empty: {
      Assumption assumption(AssumptionType::empty, std::move(lhs));
      RegularTableau::emptinessAssumptions.push_back(std::move(assumption));
      return 0;
    }
    case RelationOperation::identity: {
      Assumption assumption(AssumptionType::identity, std::move(lhs));
      RegularTableau::idAssumptions.push_back(std::move(assumption));
      return 0;
    }
    default: {
      std::cout << "[Parser] Ignoring unsupported hypothesis:" << ctx->lhs->getText()
                << " <= " << ctx->rhs->getText() << std::endl;
      return 0;
    }
  }
}

/*std::vector<Constraint>*/ std::any Logic::visitMcm(LogicParser::McmContext *context) {
  std::vector<Constraint> constraints;

  for (auto definitionContext : context->definition()) {
    if (definitionContext->letDefinition()) {
      visitLetDefinition(definitionContext->letDefinition());
    } else if (definitionContext->letRecDefinition()) {
      std::cout << "[Parser] Recursive defitions are not supported." << std::endl;
      exit(0);
    } else if (definitionContext->axiomDefinition()) {
      auto untypedAxiom = visitAxiomDefinition(definitionContext->axiomDefinition());
      Constraint axiom = std::any_cast<Constraint>(untypedAxiom);
      constraints.push_back(axiom);
    }
  }
  return constraints;
}
// use default implementation of visitDefinition(LogicParser::DefinitionContext*context)
/*Constraint*/ std::any Logic::visitAxiomDefinition(LogicParser::AxiomDefinitionContext *context) {
  std::string name;
  if (context->RELNAME()) {
    name = context->RELNAME()->getText();
  } else {
    name = context->getText();
  }
  ConstraintType type;
  if (context->EMPTY()) {
    type = ConstraintType::empty;
  } else if (context->IRREFLEXIVE()) {
    type = ConstraintType::irreflexive;
  } else if (context->ACYCLIC()) {
    type = ConstraintType::acyclic;
  }
  auto relationVariant = std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));
  Relation relation = std::get<Relation>(relationVariant);
  return Constraint(type, std::move(relation), name);
}
/*void*/ std::any Logic::visitLetDefinition(LogicParser::LetDefinitionContext *context) {
  if (context->RELNAME()) {
    std::string name = context->RELNAME()->getText();
    auto derivedRelationVariant =
        std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));
    Relation derivedRelation = std::get<Relation>(derivedRelationVariant);
    // overwrite existing definition
    Logic::definedRelations.insert_or_assign(name, derivedRelation);
  } else if (context->SETNAME()) {
    std::string name = context->SETNAME()->getText();
    // TODO: implement
  }
  return 0;
}
/*void*/ std::any Logic::visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) {
  std::cout << "[Parser] Recursive definitions are currently not supported." << std::endl;
  exit(0);
}
/*void*/ std::any Logic::visitLetRecAndDefinition(
    LogicParser::LetRecAndDefinitionContext *context) {
  std::cout << "[Parser] Recursive definitions are currently not supported." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitParentheses(
    LogicParser::ParenthesesContext *context) {
  // process: (e)
  std::variant<Set, Relation> expression =
      std::any_cast<std::variant<Set, Relation>>(context->e1->accept(this));
  return expression;
}
/*std::variant<Set, Relation>*/ std::any Logic::visitTransitiveClosure(
    LogicParser::TransitiveClosureContext *context) {
  std::variant<Set, Relation> relationExpression =
      std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));

  if (std::holds_alternative<Relation>(relationExpression)) {
    Relation r1 = std::get<Relation>(relationExpression);
    Relation r2(r1);
    Relation r1Transitive(RelationOperation::transitiveClosure, std::move(r1));
    Relation r(RelationOperation::composition, std::move(r2), std::move(r1Transitive));
    std::variant<Set, Relation> result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation transitive closure."
            << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationFencerel(
    LogicParser::RelationFencerelContext *context) {
  auto relationName = context->n->getText();
  if (Logic::definedRelations.contains(relationName)) {
    auto r = Logic::definedRelations.at(relationName);
    Relation po1(RelationOperation::base, "po");
    Relation po2(RelationOperation::base, "po");
    Relation po1_r(RelationOperation::composition, std::move(po1), std::move(r));
    Relation po1_r_po2(RelationOperation::composition, std::move(po1_r), std::move(po2));
    std::variant<Set, Relation> result = po1_r_po2;
    return result;
  }
  std::cout << "[Parser] Error: fencerel() of unkown relation." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitSetSingleton(
    LogicParser::SetSingletonContext *context) {
  std::string name = context->SETNAME()->getText();
  int label;
  if (definedSingletons.contains(name)) {
    label = definedSingletons.at(name);
  } else {
    label = Set::maxSingletonLabel++;
    definedSingletons.insert({name, label});
  }
  Set s(SetOperation::singleton, label);
  std::variant<Set, Relation> result = s;
  return result;
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationBasic(
    LogicParser::RelationBasicContext *context) {
  std::string name = context->RELNAME()->getText();
  if (name == "id") {
    std::variant<Set, Relation> result = Relation(RelationOperation::identity);
    return result;
  }
  if (name == "0") {
    std::variant<Set, Relation> result = Relation(RelationOperation::empty);
    return result;
  }
  if (Logic::definedRelations.contains(name)) {
    std::variant<Set, Relation> result = Logic::definedRelations.at(name);
    return result;
  }
  Relation r(RelationOperation::base, name);
  std::variant<Set, Relation> result = r;
  return result;
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationMinus(
    LogicParser::RelationMinusContext *context) {
  std::cout << "[Parser] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationDomainIdentity(
    LogicParser::RelationDomainIdentityContext *context) {
  std::cout << "[Parser] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationRangeIdentity(
    LogicParser::RelationRangeIdentityContext *context) {
  std::cout << "[Parser] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitUnion(LogicParser::UnionContext *context) {
  std::variant<Set, Relation> e1 =
      std::any_cast<std::variant<Set, Relation>>(context->e1->accept(this));
  std::variant<Set, Relation> e2 =
      std::any_cast<std::variant<Set, Relation>>(context->e2->accept(this));
  if (std::holds_alternative<Relation>(e1) && std::holds_alternative<Relation>(e2)) {
    Relation r1 = std::get<Relation>(e1);
    Relation r2 = std::get<Relation>(e2);
    Relation r(RelationOperation::choice, std::move(r1), std::move(r2));
    std::variant<Set, Relation> result = r;
    return result;
  }

  if (std::holds_alternative<Set>(e1) && std::holds_alternative<Set>(e2)) {
    Set s1 = std::get<Set>(e1);
    Set s2 = std::get<Set>(e2);
    Set s(SetOperation::choice, std::move(s1), std::move(s2));
    std::variant<Set, Relation> result = s;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the union operator." << std::endl;
  exit(0);
}

// TODO: emptyset as set type is not supported
/*std::variant<Set, Relation>*/ std::any Logic::visitEmptyset(LogicParser::EmptysetContext *ctx) {
  Relation r(RelationOperation::empty);
  std::variant<Set, Relation> result = r;
  return result;
}

/*std::variant<Set, Relation>*/ std::any Logic::visitRelationInverse(
    LogicParser::RelationInverseContext *context) {
  std::variant<Set, Relation> relationExpression =
      std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));
  if (std::holds_alternative<Relation>(relationExpression)) {
    Relation r1 = std::get<Relation>(relationExpression);
    Relation r(RelationOperation::converse, std::move(r1));
    std::variant<Set, Relation> result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation inverse." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationOptional(
    LogicParser::RelationOptionalContext *context) {
  std::variant<Set, Relation> relationExpression =
      std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));
  if (std::holds_alternative<Relation>(relationExpression)) {
    Relation r1 = std::get<Relation>(relationExpression);
    Relation idR(RelationOperation::identity);
    Relation r(RelationOperation::choice, std::move(r1), std::move(idR));
    std::variant<Set, Relation> result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation optional." << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitRelationIdentity(
    LogicParser::RelationIdentityContext *context) {
  if (context->TOID() == nullptr) {
    std::string set = context->e->getText();
    Relation r1(RelationOperation::base, set + "*" + set);
    Relation id(RelationOperation::identity);
    Relation r(RelationOperation::intersection, std::move(r1), std::move(id));
    std::variant<Set, Relation> result = r;
    return result;
  } else {
    std::cout << "[Parser] 'visitExprIdentity TOID' expressions are not supported." << std::endl;
    exit(0);
  }
}
/*std::variant<Set, Relation>*/ std::any Logic::visitCartesianProduct(
    LogicParser::CartesianProductContext *context) {
  /* TODO:
  std::cout << "[Parser] Cartesian products are currently not supported." << std::endl;
  exit(0);
  */
  // treat cartesian product as binary base relation
  std::string r1 = context->e1->getText();
  std::string r2 = context->e2->getText();
  Relation r(RelationOperation::base, r1 + "*" + r2);
  std::variant<Set, Relation> result = r;
  return result;
}
/*std::variant<Set, Relation>*/ std::any Logic::visitSetBasic(
    LogicParser::SetBasicContext *context) {
  // TODO: lookup let definitons
  std::string name = context->SETNAME()->getText();
  if (name == "E") {
    std::variant<Set, Relation> result = Set(SetOperation::full);
    return result;
  }
  if (name == "0") {
    std::variant<Set, Relation> result = Set(SetOperation::empty);
    return result;
  }
  Set s(SetOperation::base, name);
  std::variant<Set, Relation> result = s;
  return result;
}
/*std::variant<Set, Relation>*/ std::any Logic::visitTransitiveReflexiveClosure(
    LogicParser::TransitiveReflexiveClosureContext *context) {
  std::variant<Set, Relation> e =
      std::any_cast<std::variant<Set, Relation>>(context->e->accept(this));
  if (std::holds_alternative<Relation>(e)) {
    Relation r(RelationOperation::transitiveClosure, std::move(std::get<Relation>(e)));
    std::variant<Set, Relation> result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of operand of the Kleene operator: " << context->getText()
            << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitComposition(
    LogicParser::CompositionContext *context) {
  std::variant<Set, Relation> e1 =
      std::any_cast<std::variant<Set, Relation>>(context->e1->accept(this));
  std::variant<Set, Relation> e2 =
      std::any_cast<std::variant<Set, Relation>>(context->e2->accept(this));

  if (std::holds_alternative<Relation>(e1) && std::holds_alternative<Relation>(e2)) {
    Relation r1 = std::get<Relation>(e1);
    Relation r2 = std::get<Relation>(e2);
    Relation r(RelationOperation::composition, std::move(r1), std::move(r2));
    std::variant<Set, Relation> result = r;
    return result;
  }
  if (std::holds_alternative<Set>(e1) && std::holds_alternative<Relation>(e2)) {
    Set s1 = std::get<Set>(e1);
    Relation r2 = std::get<Relation>(e2);
    Set s(SetOperation::image, std::move(s1), std::move(r2));
    std::variant<Set, Relation> result = s;
    return result;
  }
  if (std::holds_alternative<Relation>(e1) && std::holds_alternative<Set>(e2)) {
    Relation r1 = std::get<Relation>(e1);
    Set s2 = std::get<Set>(e2);
    Set s(SetOperation::domain, std::move(s2), std::move(r1));
    std::variant<Set, Relation> result = s;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the composition operator: "
            << context->getText() << std::endl;
  exit(0);
}
/*std::variant<Set, Relation>*/ std::any Logic::visitIntersection(
    LogicParser::IntersectionContext *context) {
  std::variant<Set, Relation> e1 =
      std::any_cast<std::variant<Set, Relation>>(context->e1->accept(this));
  std::variant<Set, Relation> e2 =
      std::any_cast<std::variant<Set, Relation>>(context->e2->accept(this));
  if (std::holds_alternative<Relation>(e1) && std::holds_alternative<Relation>(e2)) {
    Relation r1 = std::get<Relation>(e1);
    Relation r2 = std::get<Relation>(e2);
    Relation r(RelationOperation::intersection, std::move(r1), std::move(r2));
    std::variant<Set, Relation> result = r;
    return result;
  }

  if (std::holds_alternative<Set>(e1) && std::holds_alternative<Set>(e2)) {
    Set s1 = std::get<Set>(e1);
    Set s2 = std::get<Set>(e2);
    Set s(SetOperation::intersection, std::move(s1), std::move(s2));
    std::variant<Set, Relation> result = s;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the intersection operator." << std::endl;
  exit(0);
}
// /*std::variant<Set, Relation>*/ std::any Logic::visitRelationComplement(
//     LogicParser::RelationComplementContext *context) {
//   std::cout << "[Parser] Complement operation is not supported." << std::endl;
//   exit(0);
// }

std::unordered_map<std::string, Relation> Logic::definedRelations;
std::unordered_map<std::string, int> Logic::definedSingletons;