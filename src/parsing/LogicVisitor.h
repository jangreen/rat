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
#include "../RegularTableau.h"
#include "../Relation.h"
#include "CatInferVisitor.h"

// helper
Relation loadModel(const std::string &file) {
  CatInferVisitor visitor;
  auto sc = visitor.parseMemoryModel(file);
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
  return *unionR;
}

typedef std::tuple<Relation, Relation> Assertion;

class Logic : LogicBaseVisitor {
 public:
  /*std::tuple<std::vector<Assumption>, std::vector<Assertion>>*/ std::any visitStatement(
      LogicParser::StatementContext *ctx) {
    for (const auto &letDefinition : ctx->letDefinition()) {
      std::string name = letDefinition->NAME()->getText();
      Relation derivedRelation(letDefinition->e->getText());
      Relation::relations.insert({name, derivedRelation});
    }

    std::vector<Assertion> assertions;
    std::vector<Assumption> hypotheses;

    for (const auto &assertionContext : ctx->assertion()) {
      auto assertion = any_cast<Assertion>(assertionContext->accept(this));
      assertions.push_back(std::move(assertion));
    }
    for (const auto &assertionMmContext : ctx->mmAssertion()) {
      auto mm = any_cast<std::tuple<Assumption, Assertion>>(assertionMmContext->accept(this));
      auto assumption = std::get<Assumption>(mm);
      auto assertion = std::get<Assertion>(mm);
      assertions.push_back(std::move(assertion));
      hypotheses.push_back(std::move(assumption));
    }

    for (const auto &hypothesisContext : ctx->hypothesis()) {
      auto hypothesis = any_cast<Assumption>(hypothesisContext->accept(this));
      hypotheses.push_back(std::move(hypothesis));
    }
    std::tuple<std::vector<Assumption>, std::vector<Assertion>> response{std::move(hypotheses),
                                                                         std::move(assertions)};
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
        std::cout << "[Parser] Unsupported hypothesis:" << ctx->lhs->getText()
                  << " <= " << ctx->rhs->getText() << std::endl;
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
    // TODO: remove  std::cout << r1.toString() << " <= " << r2.toString() << std::endl;
    Assertion response{std::move(r1), std::move(r2)};
    return response;
  }

  /*std::Assumption, Assertion>*/ std::any visitMmAssertion(LogicParser::MmAssertionContext *ctx) {
    std::string lhsPath(ctx->lhs->getText());
    std::string rhsPath(ctx->rhs->getText());
    auto lhsModel = loadModel(lhsPath);
    auto rhsModel = loadModel(rhsPath);
    // TODO: remove std::cout << rhsModel.toString() << " <= " << lhsModel.toString() << std::endl;
    Assumption lhsEmpty(AssumptionType::empty, std::move(lhsModel));
    Relation emptyR(Operation::empty);
    emptyR.negated = true;
    emptyR.label = 0;
    rhsModel.label = 0;
    Assertion rhsEmpty{std::move(rhsModel), std::move(emptyR)};
    std::tuple<Assumption, Assertion> response{std::move(lhsEmpty), std::move(rhsEmpty)};
    return response;
  }

  static std::tuple<std::vector<Assumption>, std::vector<Assertion>> parse(
      const std::string &filePath) {
    std::cout << "[Parsing] Parse file: " << filePath << std::endl;
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::StatementContext *ctx = parser.statement();
    Logic visitor;
    return any_cast<std::tuple<std::vector<Assumption>, std::vector<Assertion>>>(
        visitor.visitStatement(ctx));
  }
};
