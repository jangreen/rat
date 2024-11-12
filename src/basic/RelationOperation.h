#pragma once

enum class RelationOperation {
  baseRelation,          // nullary function (constant): base relation
  idRelation,            // nullary function (constant): identity relation
  emptyRelation,         // nullary function (constant): empty relation
  fullRelation,          // nullary function (constant): full relation
  relationUnion,         // binary function
  relationIntersection,  // binary function
  composition,           // binary function
  transitiveClosure,     // unary function
  converse,              // unary function
  setIdentity,           // unary function
  cartesianProduct       // TODO:
};

template <>
struct std::hash<RelationOperation> {
  std::size_t operator()(const RelationOperation &operation) const noexcept;
};