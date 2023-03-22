#include "CatInferVisitor.h"

#include <utility>

std::vector<Constraint> CatInferVisitor::parseMemoryModel(const std::string &filePath) {
  std::ifstream stream;
  stream.open(filePath);
  antlr4::ANTLRInputStream input(stream);

  CatLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  CatParser parser(&tokens);

  CatParser::McmContext *ctx = parser.mcm();
  return any_cast<std::vector<Constraint>>(this->visitMcm(ctx));
}

Relation CatInferVisitor::parseRelation(const std::string &relationString) {
  antlr4::ANTLRInputStream input(relationString);
  CatLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  CatParser parser(&tokens);

  CatParser::ExpressionContext *ctx = parser.expression();  // expect expression
  Relation parsedRelation = any_cast<Relation>(this->visit(ctx));
  return parsedRelation;
}

/*std::vector<Constraint>*/ antlrcpp::Any CatInferVisitor::visitMcm(CatParser::McmContext *ctx) {
  std::vector<Constraint> constraints;

  for (auto definitionContext : ctx->definition()) {
    if (definitionContext->letDefinition()) {
      definitionContext->letDefinition()->accept(this);
    } else if (definitionContext->letRecDefinition()) {
      std::cout << "TODO: recursive definitions" << std::endl;
    } else if (definitionContext->axiomDefinition()) {
      Constraint axiom = any_cast<Constraint>(definitionContext->axiomDefinition()->accept(this));
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
  Relation relation = any_cast<Relation>(ctx->e->accept(this));
  return Constraint(type, std::move(relation), name);
}

/*void*/ antlrcpp::Any CatInferVisitor::visitLetDefinition(CatParser::LetDefinitionContext *ctx) {
  std::string name = ctx->NAME()->getText();
  Relation derivedRelation = any_cast<Relation>(ctx->e->accept(this));
  // Relation::relations[name] = derivedRelation; // TODO: why error?
  Relation::relations.insert({name, derivedRelation});
  return antlrcpp::Any();
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitExpr(CatParser::ExprContext *ctx) {
  // process: (e)
  Relation derivedRelation = any_cast<Relation>(ctx->e->accept(this));
  return derivedRelation;
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprCartesian(
    CatParser::ExprCartesianContext *ctx) {
  // treat cartesian product as binary base relation
  std::string r1 = ctx->e1->getText();
  std::string r2 = ctx->e2->getText();
  return Relation(Operation::base, r1 + "*" + r2);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprBasic(CatParser::ExprBasicContext *ctx) {
  std::string name = ctx->NAME()->getText();
  if (name == "id") {
    return Relation(Operation::identity);
  }
  if (name == "0") {
    return Relation(Operation::empty);
  }
  if (Relation::relations.contains(name)) {
    return Relation::relations.at(name);
  }
  return Relation(Operation::base, name);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprMinus(CatParser::ExprMinusContext *ctx) {
  std::cout << "[Parsing] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprUnion(CatParser::ExprUnionContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
  return Relation(Operation::choice, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprComposition(
    CatParser::ExprCompositionContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
  return Relation(Operation::composition, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprIntersection(
    CatParser::ExprIntersectionContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
  Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
  return Relation(Operation::intersection, std::move(r1), std::move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprTransitive(
    CatParser::ExprTransitiveContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e->accept(this));
  Relation r2(r1);
  Relation r1Transitive(Operation::transitiveClosure, std::move(r1));
  return Relation(Operation::composition, std::move(r2), std::move(r1Transitive));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprComplement(
    CatParser::ExprComplementContext *ctx) {
  std::cout << "[Parsing] Complement operation is not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprInverse(CatParser::ExprInverseContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e->accept(this));
  return Relation(Operation::converse, std::move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprDomainIdentity(
    CatParser::ExprDomainIdentityContext *ctx) {
  std::cout << "[Parsing] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprIdentity(CatParser::ExprIdentityContext *ctx) {
  if (ctx->TOID() == nullptr) {
    std::string set = ctx->e->getText();
    Relation r1(Operation::base, set + "*" + set);
    Relation id(Operation::identity);
    return Relation(Operation::intersection, std::move(r1), std::move(id));
  } else {
    std::cout << "[Parsing] 'visitExprIdentity TOID' expressions are not supported." << std::endl;
    exit(0);
  }
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprRangeIdentity(
    CatParser::ExprRangeIdentityContext *ctx) {
  std::cout << "[Parsing] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprTransRef(CatParser::ExprTransRefContext *ctx) {
  Relation r1 = any_cast<Relation>(ctx->e->accept(this));
  return Relation(Operation::transitiveClosure, std::move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprFencerel(CatParser::ExprFencerelContext *ctx) {
  std::cout << "[Parsing] Fence expressions are not supported." << std::endl;
  exit(0);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprOptional(CatParser::ExprOptionalContext *ctx) {
  std::cout << "[Parsing] Optional expressions are not supported." << std::endl;
  exit(0);
}
