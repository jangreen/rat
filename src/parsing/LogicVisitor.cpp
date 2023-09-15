#include "LogicVisitor.h"

std::vector<Constraint> Logic::parseMemoryModel(const std::string &filePath) {
  std::cout << "[Parsing] Parse file: " << filePath << std::endl;
  std::ifstream stream;
  stream.open(filePath);
  antlr4::ANTLRInputStream input(stream);

  LogicLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  LogicParser parser(&tokens);

  LogicParser::McmContext *ctx = parser.mcm();
  return std::any_cast<std::vector<Constraint>>(this->visitMcm(ctx));
}

Relation Logic::parseRelation(const std::string &relationString) {
  antlr4::ANTLRInputStream input(relationString);
  LogicLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  LogicParser parser(&tokens);

  LogicParser::RelationExpressionContext *ctx = parser.relationExpression();  // expect expression
  Relation parsedRelation = std::any_cast<Relation>(this->visit(ctx));
  return parsedRelation;
}

/*std::vector<Formula>*/ std::any Logic::visitProof(LogicParser::ProofContext *context) {
  for (const auto &letDefinition : ctx->letDefinition()) {
    std::string name = letDefinition->NAME()->getText();
    Relation derivedRelation(letDefinition->e->getText());
    Logic::definedRelations.insert({name, derivedRelation});
  }

  std::vector<Formula> formulas;

  for (const auto &assertionContext : ctx->assertion()) {
    auto formula = std::any_cast<Formula>(this->visitAssertion(assertionContext));
    formulas.push_back(std::move(formula));
  }

  return formulas;
}
/*Formula*/ std::any Logic::visitAssertion(LogicParser::AssertionContext *context) {
  Formula f(ctx->f1->getText());
  return f;
}
/*Formula*/ std::any Logic::visitFormula(LogicParser::FormulaContext *context) {}
/*Predicate*/ std::any Logic::visitPredicate(LogicParser::PredicateContext *context) {}
/*std::vector<Constraint>*/ std::any Logic::visitMcm(LogicParser::McmContext *context) {
  std::vector<Constraint> constraints;

  for (auto definitionContext : ctx->definition()) {
    if (definitionContext->letDefinition()) {
      definitionContext->letDefinition()->accept(this);
    } else if (definitionContext->letRecDefinition()) {
      std::cout << "[Parsing] Recursive defitions are not supported." << std::endl;
      exit(0);
    } else if (definitionContext->axiomDefinition()) {
      Constraint axiom =
          std::any_cast<Constraint>(definitionContext->axiomDefinition()->accept(this));
      constraints.push_back(axiom);
    }
  }
  return constraints;
}
/*void*/ std::any Logic::visitDefinition(LogicParser::DefinitionContext *context) {}
/*Constraint*/ std::any Logic::visitAxiomDefinition(LogicParser::AxiomDefinitionContext *context) {
  std::string name;
  if (ctx->NAME()) {
    name = ctx->NAME()->getText();
  } else {
    name = ctx->getText();
  }
  ConstraintType type;
  if (ctx->EMPTY()) {
    type = ConstraintType::empty;
  } else if (ctx->IRREFLEXIVE()) {
    type = ConstraintType::irreflexive;
  } else if (ctx->ACYCLIC()) {
    type = ConstraintType::acyclic;
  }
  Relation relation = std::any_cast<Relation>(ctx->e->accept(this));
  return Constraint(type, std::move(relation), name);
}
/*void*/ std::any Logic::visitLetDefinition(LogicParser::LetDefinitionContext *context) {
  std::string name = ctx->NAME()->getText();
  Relation derivedRelation = std::any_cast<Relation>(ctx->e->accept(this));
  Logic::definedRelations.insert({name, derivedRelation});
  return antlrcpp::Any();
}
/*void*/ std::any Logic::visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) {}
/*void*/ std::any Logic::visitLetRecAndDefinition(
    LogicParser::LetRecAndDefinitionContext *context) {}
/*std::variant<Set, Relation>*/ std::any Logic::visitParentheses(
    LogicParser::ParenthesesContext *context) {
  // process: (e)
  Relation derivedRelation = std::any_cast<Relation>(context->e->accept(this));
  return derivedRelation;
}
/*Relation*/ std::any Logic::visitTransitiveClosure(
    LogicParser::TransitiveClosureContext *context) {}
/*Relation*/ std::any Logic::visitRelationFencerel(LogicParser::RelationFencerelContext *context) {}
/*Set*/ std::any Logic::visitSetSingleton(LogicParser::SetSingletonContext *context) {}
/*Relation*/ std::any Logic::visitRelationBasic(LogicParser::RelationBasicContext *context) {}
/*Relation*/ std::any Logic::visitRelationMinus(LogicParser::RelationMinusContext *context) {}
/*Relation*/ std::any Logic::visitRelationDomainIdentity(
    LogicParser::RelationDomainIdentityContext *context) {}
/*Relation*/ std::any Logic::visitRelationRangeIdentity(
    LogicParser::RelationRangeIdentityContext *context) {}
/*std::variant<Set, Relation>*/ std::any Logic::visitUnion(LogicParser::UnionContext *context) {}
/*Relation*/ std::any Logic::visitRelationInverse(LogicParser::RelationInverseContext *context) {}
/*Relation*/ std::any Logic::visitRelationOptional(LogicParser::RelationOptionalContext *context) {}
/*Relation*/ std::any Logic::visitRelationIdentity(LogicParser::RelationIdentityContext *context) {}
/*Relation*/ std::any Logic::visitCartesianProduct(LogicParser::CartesianProductContext *context) {}
/*Set*/ std::any Logic::visitSetBasic(LogicParser::SetBasicContext *context) {}
/*Relation*/ std::any Logic::visitTransitiveReflexiveClosure(
    LogicParser::TransitiveReflexiveClosureContext *context) {}
/*std::variant<Set, Relation>*/ std::any Logic::visitComposition(
    LogicParser::CompositionContext *context) {}
/*std::variant<Set, Relation>*/ std::any Logic::visitIntersection(
    LogicParser::IntersectionContext *context) {}
/*Relation*/ std::any Logic::visitRelationComplement(
    LogicParser::RelationComplementContext *context) {}

/*Relation*/ antlrcpp::Any Logic::visitCartesianProduct(LogicParser::CartesianProductContext *ctx) {
  // treat cartesian product as binary base relation
  std::string r1 = ctx->e1->getText();
  std::string r2 = ctx->e2->getText();
  Relation cartesianProduct(RelationOperation::base, r1 + "*" + r2);
  Relation id(RelationOperation::identity);
  return Relation(RelationOperation::intersection, std::move(cartesianProduct), std::move(id));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationBasic(LogicParser::RelationBasicContext *ctx) {
  std::string name = ctx->NAME()->getText();
  if (name == "id") {
    return Relation(RelationOperation::identity);
  }
  if (name == "0") {
    return Relation(RelationOperation::empty);
  }
  if (Logic::definedRelations.contains(name)) {
    return Logic::definedRelations.at(name);
  }
  return Relation(RelationOperation::base, name);
}
/*Relation*/ antlrcpp::Any Logic::visitRelationMinus(LogicParser::RelationMinusContext *ctx) {
  std::cout << "[Parsing] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any Logic::visitRelationUnion(LogicParser::RelationUnionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::choice, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationComposition(
    LogicParser::RelationCompositionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::composition, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationIntersection(
    LogicParser::RelationIntersectionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::intersection, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any Logic::visitTransitiveClosure(
    LogicParser::TransitiveClosureContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  Relation r2(r1);
  Relation r1Transitive(RelationOperation::transitiveClosure, std::move(r1));
  return Relation(RelationOperation::composition, std::move(r2), std::move(r1Transitive));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationComplement(
    LogicParser::RelationComplementContext *ctx) {
  std::cout << "[Parsing] Complement operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any Logic::visitRelationInverse(LogicParser::RelationInverseContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  return Relation(RelationOperation::converse, std::move(r1));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationDomainIdentity(
    LogicParser::RelationDomainIdentityContext *ctx) {
  std::cout << "[Parsing] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any Logic::visitRelationIdentity(LogicParser::RelationIdentityContext *ctx) {
  if (ctx->TOID() == nullptr) {
    std::string set = ctx->e->getText();
    Relation r1(RelationOperation::base, set + "*" + set);
    Relation id(RelationOperation::identity);
    return Relation(RelationOperation::intersection, std::move(r1), std::move(id));
  } else {
    std::cout << "[Parsing] 'visitExprIdentity TOID' expressions are not supported." << std::endl;
    exit(0);
  }
}
/*Relation*/ antlrcpp::Any Logic::visitRelationRangeIdentity(
    LogicParser::RelationRangeIdentityContext *ctx) {
  std::cout << "[Parsing] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any Logic::visitTransReflexiveClosure(
    LogicParser::TransReflexiveClosureContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  return Relation(RelationOperation::transitiveClosure, std::move(r1));
}
/*Relation*/ antlrcpp::Any Logic::visitRelationFencerel(LogicParser::RelationFencerelContext *ctx) {
  std::cout << "[Parsing] Fence expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any Logic::visitRelationOptional(LogicParser::RelationOptionalContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  Relation idR(RelationOperation::identity);
  return Relation(RelationOperation::choice, std::move(r1), std::move(idR));
}

/*Set*/ antlrcpp::Any Logic::visitSetBasic(LogicParser::SetBasicContext *ctx) {
  // TODO: lookup let definitons
  std::string name = ctx->NAME()->getText();
  if (name == "E") {
    return Set(SetOperation::full);
  }
  if (name == "0") {
    return Set(SetOperation::empty);
  }
  return Set(SetOperation::base, name);
}

/*Set*/ antlrcpp::Any Logic::visitSingleton(LogicParser::SingletonContext *ctx) {
  std::string name = ctx->NAME()->getText();
  int label;
  if (definedSingletons.contains(name)) {
    label = definedSingletons.at(name);
  } else {
    label = Set::maxSingletonLabel++;
    definedSingletons.insert({name, label});
  }
  Set s(SetOperation::singleton, label);
  return s;
}

/*Set*/ antlrcpp::Any Logic::visitSetIntersection(LogicParser::SetIntersectionContext *ctx) {
  Set s1 = std::any_cast<Set>(ctx->e1->accept(this));
  Set s2 = std::any_cast<Set>(ctx->e2->accept(this));
  Set s(SetOperation::intersection, std::move(s1), std::move(s2));
  return s;
}

/*Set*/ antlrcpp::Any Logic::visitSetUnion(LogicParser::SetUnionContext *ctx) {
  Set s1 = std::any_cast<Set>(ctx->e1->accept(this));
  Set s2 = std::any_cast<Set>(ctx->e2->accept(this));
  Set s(SetOperation::choice, std::move(s1), std::move(s2));
  return s;
}

/*Set*/ antlrcpp::Any Logic::visitSet(LogicParser::SetContext *ctx) {
  // process: (S1)
  return std::any_cast<Set>(ctx->e1->accept(this));
}

/* Formula */ std::any Logic::visitFormula(LogicParser::FormulaContext *ctx) {
  if (ctx->NOT()) {
    auto f = std::any_cast<Formula>(this->visitFormula(ctx->f1));
    Formula not_f(FormulaOperation::negation, std::move(f));
    return not_f;
  } else if (ctx->AMP()) {
    auto f1 = std::any_cast<Formula>(this->visitFormula(ctx->f1));
    auto f2 = std::any_cast<Formula>(this->visitFormula(ctx->f2));
    Formula f1_and_f2(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
    return f1_and_f2;
  } else if (ctx->BAR()) {
    auto f1 = std::any_cast<Formula>(this->visitFormula(ctx->f1));
    auto f2 = std::any_cast<Formula>(this->visitFormula(ctx->f2));
    Formula f1_or_f2(FormulaOperation::logicalOr, std::move(f1), std::move(f2));
    return f1_or_f2;
  } else {
    auto p = std::any_cast<Predicate>(this->visitPredicate(ctx->p1));
    Literal l(false, std::move(p));
    Formula f(FormulaOperation::literal, std::move(l));
    return f;
  }
}

/* Predicate */ std::any Logic::visitPredicate(LogicParser::PredicateContext *ctx) {
  auto s1 = std::any_cast<Set>(this->Logic::visit(ctx->s1));
  auto s2 = std::any_cast<Set>(this->Logic::visit(ctx->s2));
  Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(s1), std::move(s2));
  return p;
}

std::unordered_map<std::string, Relation> Logic::definedRelations;
std::unordered_map<std::string, int> Logic::definedSingletons;