#pragma once
#include <LogicBaseVisitor.h>
#include <LogicLexer.h>
#include <LogicParser.h>
#include <antlr4-runtime.h>

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../Assumption.h"
#include "CatInferVisitor.h"
#include "../RegularTableau.h"
#include "../Relation.h"

// helper
Relation loadModel(const std::string &file) {
  CatInferVisitor visitor;
  auto sc = visitor.parseMemoryModel("../cat/" + file);
  std::optional<Relation> unionR;
  for (auto &constraint : sc) {
    constraint.toEmptyNormalForm();
    Relation r = constraint.relation;
    if (unionR) {
      unionR = Relation(Operation::choice, std::move(*unionR), std::move(r));
    } else {
      unionR = r;
    }
  }
  unionR->label = 0;
  return *unionR;
}

typedef std::tuple<Relation, Relation> Assertion;

class Logic : LogicBaseVisitor {
 public:
  /*std::tuple<std::vector<Assumption>, Assertion>*/ std::any visitStatement(
      LogicParser::StatementContext *ctx) {
    for (const auto &letDefinition : ctx->letDefinition()) {
      std::string name = letDefinition->NAME()->getText();
      Relation derivedRelation(letDefinition->e->getText());
      Relation::relations.insert({name, derivedRelation});
    }

    std::optional<Assertion> assertion;
    const auto &assertionContext = ctx->assertion();
    if (assertionContext != nullptr) {
      assertion = any_cast<Assertion>(assertionContext->accept(this));
    } else {
      const auto &assertionMmContext = ctx->mmAssertion();
      assertion = any_cast<Assertion>(assertionMmContext->accept(this));
    }

    std::vector<Assumption> hypotheses;
    for (const auto &hypothesisContext : ctx->hypothesis()) {
      auto hypothesis = any_cast<Assumption>(hypothesisContext->accept(this));
      hypotheses.push_back(std::move(hypothesis));
    }
    std::tuple<std::vector<Assumption>, Assertion> response{std::move(hypotheses),
                                                            std::move(*assertion)};
    return response;
  }

  /*Assumption*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) {
    Relation lhs(ctx->lhs->getText());
    Relation rhs(ctx->rhs->getText());
    switch (rhs.operation) {
      case Operation::base:
        return Assumption(AssumptionType::regular, std::move(lhs), *rhs.identifier);
      case Operation::empty:
        return Assumption(AssumptionType::empty, std::move(lhs));
      case Operation::identity:
        return Assumption(AssumptionType::identity, std::move(lhs));
      default:
        break;
    }
  }

  /*Assertion*/ std::any visitAssertion(LogicParser::AssertionContext *ctx) {
    Relation r1(ctx->lhs->getText());
    Relation r2(ctx->rhs->getText());
    r1.label = 0;
    r1.negated = false;
    r2.label = 0;
    r2.negated = true;
    Assertion response{std::move(r1), std::move(r2)};
    return response;
  }

  /*Assertion*/ std::any visitMmAssertion(LogicParser::MmAssertionContext *ctx) {
    std::string lhs(ctx->lhs->getText());
    std::string rhs(ctx->rhs->getText());
    auto lhsModel = loadModel(lhs + ".cat");
    auto rhsModel = loadModel(rhs + ".cat");
    lhsModel.negated = true;
    Assertion response{std::move(rhsModel), std::move(lhsModel)};
    return response;
  }

  static std::tuple<std::vector<Assumption>, Assertion> parse(const std::string &filePath) {
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::StatementContext *ctx = parser.statement();
    Logic visitor;
    return any_cast<std::tuple<std::vector<Assumption>, Assertion>>(visitor.visitStatement(ctx));
  }
};
