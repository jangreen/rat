#pragma once
#include <LogicBaseVisitor.h>
#include <LogicLexer.h>
#include <LogicParser.h>
#include <antlr4-runtime.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../Literal.h"
#include "../cat/Constraint.h"
#include "LogicVisitor.h"

/*/ helper
Relation loadModel(const std::string &file) {
  auto sc = Logic::parseMemoryModel(file);
  std::optional<Relation> unionR;
  for (auto &constraint : sc) {
    constraint.toEmptyNormalForm();
    Relation r = constraint.relation;
    if (unionR) {
      unionR = Relation(RelationOperation::choice, std::move(*unionR), std::move(r));
    } else {
      unionR = r;
    }
  }
  return *unionR;
}
*/
class Logic : LogicBaseVisitor {
 public:
  /*DNF*/ std::any visitProof(LogicParser::ProofContext *context) override;
  // /*void*/ std::any visitStatement(LogicParser::StatementContext *ctx) override;
  /*void*/ std::any visitInclusion(LogicParser::InclusionContext *ctx) override;
  /*Cube*/ std::any visitAssertion(LogicParser::AssertionContext *context) override;
  /*void*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) override;
  /*std::vector<Constraint>*/ std::any visitMcm(LogicParser::McmContext *context) override;
  // /*void*/ std::any visitDefinition(LogicParser::DefinitionContext *context) override;
  /*Constraint*/ std::any visitAxiomDefinition(
      LogicParser::AxiomDefinitionContext *context) override;
  /*void*/ std::any visitLetDefinition(LogicParser::LetDefinitionContext *context) override;
  /*void*/ std::any visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) override;
  /*void*/ std::any visitLetRecAndDefinition(
      LogicParser::LetRecAndDefinitionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitParentheses(
      LogicParser::ParenthesesContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitTransitiveClosure(
      LogicParser::TransitiveClosureContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationFencerel(
      LogicParser::RelationFencerelContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitSetSingleton(
      LogicParser::SetSingletonContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationBasic(
      LogicParser::RelationBasicContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationMinus(
      LogicParser::RelationMinusContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationDomainIdentity(
      LogicParser::RelationDomainIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationRangeIdentity(
      LogicParser::RelationRangeIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitUnion(LogicParser::UnionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitEmptyset(
      LogicParser::EmptysetContext *ctx) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationInverse(
      LogicParser::RelationInverseContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationOptional(
      LogicParser::RelationOptionalContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationIdentity(
      LogicParser::RelationIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitCartesianProduct(
      LogicParser::CartesianProductContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitSetBasic(
      LogicParser::SetBasicContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitTransitiveReflexiveClosure(
      LogicParser::TransitiveReflexiveClosureContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitComposition(
      LogicParser::CompositionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitIntersection(
      LogicParser::IntersectionContext *context) override;
  // /*std::variant<Set, Relation>*/ std::any visitRelationComplement(
  //    LogicParser::RelationComplementContext *context) override;

  static std::unordered_map<std::string, Relation> definedRelations;
  static std::unordered_map<std::string, int> definedSingletons;
  static DNF parse(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] File: {}", filePath));
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ProofContext *ctx = parser.proof();
    Logic visitor;
    return std::any_cast<DNF>(visitor.visitProof(ctx));
  }
  static std::vector<Constraint> parseMemoryModel(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] File: {}", filePath));
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::McmContext *context = parser.mcm();
    Logic visitor;
    return std::any_cast<std::vector<Constraint>>(visitor.visitMcm(context));
  }

  static Relation parseRelation(const std::string &relationString) {
    antlr4::ANTLRInputStream input(relationString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *context = parser.expression();  // expect expression
    Logic visitor;
    std::variant<Set, Relation> parsedRelation =
        std::any_cast<std::variant<Set, Relation>>(visitor.visit(context));
    return std::get<Relation>(parsedRelation);
  }

  static Set parseSet(const std::string &setString) {
    antlr4::ANTLRInputStream input(setString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *ctx = parser.expression();
    Logic visitor;
    std::variant<Set, Relation> set =
        std::any_cast<std::variant<Set, Relation>>(visitor.visit(ctx));
    return std::get<Set>(set);
  }
};
