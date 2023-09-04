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
#include "../Formula.h"
#include "../Predicate.h"
#include "../RegularTableau.h"
#include "../Relation.h"
#include "../Set.h"
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
      unionR = Relation(RelationOperation::choice, std::move(*unionR), std::move(r));
    } else {
      unionR = r;
    }
  }
  return *unionR;
}

class Logic : LogicBaseVisitor {
 public:
  /*std::tuple<std::vector<Assumption>, std::vector<Formula>>*/ std::any visitStatement(
      LogicParser::StatementContext *ctx) {
    for (const auto &letDefinition : ctx->letDefinition()) {
      std::string name = letDefinition->NAME()->getText();
      Relation derivedRelation(letDefinition->e->getText());
      Relation::relations.insert({name, derivedRelation});
    }

    std::vector<Formula> formulas;
    std::vector<Assumption> hypotheses;

    for (const auto &assertionContext : ctx->assertion()) {
      auto formula = any_cast<Formula>(assertionContext->accept(this));
      formulas.push_back(std::move(formula));
    }
    for (const auto &assertionMmContext : ctx->mmAssertion()) {
      auto mm = any_cast<std::tuple<Assumption, Formula>>(assertionMmContext->accept(this));
      auto assumption = std::get<Assumption>(mm);
      auto formula = std::get<Formula>(mm);
      formulas.push_back(std::move(formula));
      hypotheses.push_back(std::move(assumption));
    }

    for (const auto &hypothesisContext : ctx->hypothesis()) {
      auto hypothesis = any_cast<Assumption>(hypothesisContext->accept(this));
      hypotheses.push_back(std::move(hypothesis));
    }
    std::tuple<std::vector<Assumption>, std::vector<Formula>> response{std::move(hypotheses),
                                                                       std::move(formulas)};
    return response;
  }

  /*Assumption*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) {
    Relation lhs(ctx->lhs->getText());
    Relation rhs(ctx->rhs->getText());
    switch (rhs.operation) {
      case RelationOperation::base:
        return Assumption(AssumptionType::regular, std::move(lhs), *rhs.identifier);
      case RelationOperation::empty:
        return Assumption(AssumptionType::empty, std::move(lhs));
      case RelationOperation::identity:
        return Assumption(AssumptionType::identity, std::move(lhs));
      default:
        std::cout << "[Parser] Unsupported hypothesis:" << ctx->lhs->getText()
                  << " <= " << ctx->rhs->getText() << std::endl;
        break;
    }
  }

  /*Formula*/ std::any visitAssertion(LogicParser::AssertionContext *ctx) {
    Relation r1(ctx->lhs->getText());
    Relation r2(ctx->rhs->getText());
    Set startEvent1(SetOperation::singleton, Set::maxSingletonLabel++);
    Set endEvent1(SetOperation::singleton, Set::maxSingletonLabel++);
    Set startEvent2(startEvent1);
    Set endEvent2(endEvent1);
    Set image1(SetOperation::image, std::move(startEvent1), std::move(r1));
    Set image2(SetOperation::image, std::move(startEvent2), std::move(r2));
    Predicate p1(PredicateOperation::intersectionNonEmptiness, std::move(image1),
                 std::move(endEvent1));
    Predicate p2(PredicateOperation::intersectionNonEmptiness, std::move(image2),
                 std::move(endEvent2));
    Literal l1(false, std::move(p1));
    Literal l2(true, std::move(p2));
    Formula f1(FormulaOperation::literal, std::move(l1));
    Formula f2(FormulaOperation::literal, std::move(l2));
    Formula response(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
    return response;
  }

  /*std::Assumption, Formula>*/ std::any visitMmAssertion(LogicParser::MmAssertionContext *ctx) {
    std::string lhsPath(ctx->lhs->getText());
    std::string rhsPath(ctx->rhs->getText());
    auto lhsModel = loadModel(lhsPath);
    auto rhsModel = loadModel(rhsPath);
    // TODO: remove std::cout << rhsModel.toString() << " <= " << lhsModel.toString() << std::endl;
    Assumption lhsEmpty(AssumptionType::empty, std::move(lhsModel));
    Relation emptyR(RelationOperation::empty);
    Set startLabel(SetOperation::singleton, Set::maxSingletonLabel++);
    Set endLabel(SetOperation::singleton, Set::maxSingletonLabel++);
    Set image(SetOperation::image, std::move(startLabel), std::move(rhsModel));
    Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(image),
                std::move(endLabel));
    Literal l(false, std::move(p));
    Formula f(FormulaOperation::literal, std::move(l));
    std::tuple<Assumption, Formula> response{std::move(lhsEmpty), std::move(f)};
    return response;
  }

  static std::tuple<std::vector<Assumption>, std::vector<Formula>> parse(
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
    return any_cast<std::tuple<std::vector<Assumption>, std::vector<Formula>>>(
        visitor.visitStatement(ctx));
  }
};
