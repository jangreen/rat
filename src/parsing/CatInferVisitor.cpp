#include "CatInferVisitor.h"

#include <utility>

std::vector<Constraint> CatInferVisitor::parseMemoryModel(const std::string &filePath) {
  std::cout << "[Parsing] Parse file: " << filePath << std::endl;
  std::ifstream stream;
  stream.open(filePath);
  antlr4::ANTLRInputStream input(stream);

  CatLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  CatParser parser(&tokens);

  CatParser::McmContext *ctx = parser.mcm();
  return std::any_cast<std::vector<Constraint>>(this->visitMcm(ctx));
}

Relation CatInferVisitor::parseRelation(const std::string &relationString) {
  antlr4::ANTLRInputStream input(relationString);
  CatLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  CatParser parser(&tokens);

  CatParser::RelationExpressionContext *ctx = parser.relationExpression();  // expect expression
  Relation parsedRelation = std::any_cast<Relation>(this->visit(ctx));
  return parsedRelation;
}

/*std::vector<Constraint>*/ antlrcpp::Any CatInferVisitor::visitMcm(CatParser::McmContext *ctx) {
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

/*Constraint*/ antlrcpp::Any CatInferVisitor::visitAxiomDefinition(
    CatParser::AxiomDefinitionContext *ctx) {
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

/*void*/ antlrcpp::Any CatInferVisitor::visitLetDefinition(CatParser::LetDefinitionContext *ctx) {
  std::string name = ctx->NAME()->getText();
  Relation derivedRelation = std::any_cast<Relation>(ctx->e->accept(this));
  Relation::relations.insert({name, derivedRelation});
  return antlrcpp::Any();
}

/*Set*/ antlrcpp::Any visitSetBasic(CatParser::SetBasicContext *ctx) {
  std::string name = ctx->NAME()->getText();
  if (name == "E") {
    return Set(SetOperation::full);
  }
  if (name == "0") {
    return Set(SetOperation::empty);
  }
  return Set(SetOperation::base, name);
}

/*Set*/ antlrcpp::Any visitSetIntersection(CatParser::SetBasicContext *ctx) {
  std::cout << "[Parsing] Set intersection operation is not supported." << std::endl;
  exit(0);
}

/*Set*/ antlrcpp::Any visitSetUnion(CatParser::SetBasicContext *ctx) {
  std::cout << "[Parsing] Set union operation is not supported." << std::endl;
  exit(0);
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelation(CatParser::RelationContext *ctx) {
  // process: (e)
  Relation derivedRelation = std::any_cast<Relation>(ctx->e->accept(this));
  return derivedRelation;
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitCartesianProduct(
    CatParser::CartesianProductContext *ctx) {
  // treat cartesian product as binary base relation
  std::string r1 = ctx->e1->getText();
  std::string r2 = ctx->e2->getText();
  Relation cartesianProduct(RelationOperation::base, r1 + "*" + r2);
  Relation id(RelationOperation::identity);
  return Relation(RelationOperation::intersection, std::move(cartesianProduct), std::move(id));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationBasic(
    CatParser::RelationBasicContext *ctx) {
  std::string name = ctx->NAME()->getText();
  if (name == "id") {
    return Relation(RelationOperation::identity);
  }
  if (name == "0") {
    return Relation(RelationOperation::empty);
  }
  if (Relation::relations.contains(name)) {
    return Relation::relations.at(name);
  }
  return Relation(RelationOperation::base, name);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationMinus(
    CatParser::RelationMinusContext *ctx) {
  std::cout << "[Parsing] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationUnion(
    CatParser::RelationUnionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::choice, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationComposition(
    CatParser::RelationCompositionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::composition, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationIntersection(
    CatParser::RelationIntersectionContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = std::any_cast<Relation>(ctx->e2->accept(this));
  return Relation(RelationOperation::intersection, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitTransitiveClosure(
    CatParser::TransitiveClosureContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  Relation r2(r1);
  Relation r1Transitive(RelationOperation::transitiveClosure, std::move(r1));
  return Relation(RelationOperation::composition, std::move(r2), std::move(r1Transitive));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationComplement(
    CatParser::RelationComplementContext *ctx) {
  std::cout << "[Parsing] Complement operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationInverse(
    CatParser::RelationInverseContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  return Relation(RelationOperation::converse, std::move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationDomainIdentity(
    CatParser::RelationDomainIdentityContext *ctx) {
  std::cout << "[Parsing] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationIdentity(
    CatParser::RelationIdentityContext *ctx) {
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
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationRangeIdentity(
    CatParser::RelationRangeIdentityContext *ctx) {
  std::cout << "[Parsing] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitTransReflexiveClosure(
    CatParser::TransReflexiveClosureContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  return Relation(RelationOperation::transitiveClosure, std::move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationFencerel(
    CatParser::RelationFencerelContext *ctx) {
  std::cout << "[Parsing] Fence expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitRelationOptional(
    CatParser::RelationOptionalContext *ctx) {
  Relation r1 = std::any_cast<Relation>(ctx->e->accept(this));
  Relation idR(RelationOperation::identity);
  return Relation(RelationOperation::choice, std::move(r1), std::move(idR));
}

/*Set*/ antlrcpp::Any CatInferVisitor::visitSetBasic(CatParser::SetBasicContext *ctx) {
  std::string name = ctx->NAME()->getText();
  Set s(SetOperation::base, name);
  return s;
}

/*Set*/ antlrcpp::Any CatInferVisitor::visitSingleton(CatParser::SingletonContext *ctx) {
  std::string name = ctx->NAME()->getText();
  Set s(SetOperation::singleton, name);
  return s;
}

/*Set*/ antlrcpp::Any CatInferVisitor::visitSetIntersection(
    CatParser::SetIntersectionContext *ctx) {
  Set s1 = std::any_cast<Set>(ctx->e1->accept(this));
  Set s2 = std::any_cast<Set>(ctx->e2->accept(this));
  Set s(SetOperation::intersection, std::move(s1), std::move(s2));
  return s;
}

/*Set*/ antlrcpp::Any CatInferVisitor::visitSetUnion(CatParser::SetUnionContext *ctx) {
  Set s1 = std::any_cast<Set>(ctx->e1->accept(this));
  Set s2 = std::any_cast<Set>(ctx->e2->accept(this));
  Set s(SetOperation::choice, std::move(s1), std::move(s2));
  return s;
}