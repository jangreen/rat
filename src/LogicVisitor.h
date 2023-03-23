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

#include "Assumption.h"
#include "RegularTableau.h"
#include "Relation.h"

typedef std::tuple<Relation, Relation> Assertion;

class Logic : LogicBaseVisitor {
 public:
  /*std::tuple<std::vector<Assumption>, Assertion>*/ std::any visitStatement(
      LogicParser::StatementContext *ctx) {
    std::optional<Assertion> assertion;
    for (const auto &assertionContext : ctx->assertion()) {
      assertion = any_cast<Assertion>(assertionContext->accept(this));
      // TODO: currently only one assertion is supported
      break;
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
