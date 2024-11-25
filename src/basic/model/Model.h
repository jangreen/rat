#pragma once

#include "../Literal.h"
#include "Edge.h"
#include "Event.h"

class Model {
  EventSet events;
  std::unordered_map<std::string, SetValue> baseSets;
  std::unordered_map<std::string, RelationValue> baseRelations;
  IdentitiesValue identities;

  [[nodiscard]] EventSet getEquivalenceClass(const EventType &event) const;

 public:
  explicit Model(const Cube &cube);

  [[nodiscard]] bool evaluate(const Literal &literal) const;
  [[nodiscard]] InterpretationPtr evaluate(CanonicalSet set) const;
  [[nodiscard]] InterpretationPtr evaluate(CanonicalRelation relation) const;

  bool addBaseSet(const std::string &baseSet, const Event &event);
  bool addBaseRelation(const std::string &baseRelation, const Edge &edge);
  bool addIdentity(const Edge &edge);

  [[nodiscard]] std::optional<CanonicalSet> getReason(const std::string &baseSet,
                                                      EventType event) const;
  [[nodiscard]] std::optional<CanonicalRelation> getReason(const std::string &baseRelation,
                                                           EventType from, EventType to) const;
  [[nodiscard]] std::optional<CanonicalRelation> getReason(EventType from, EventType to) const;

  [[nodiscard]] bool baseSetContains(const std::string &baseSet, EventType event) const;
  [[nodiscard]] bool baseRelationContains(const std::string &baseRelation, EventType from,
                                          EventType to) const;
  [[nodiscard]] bool containsIdentity(EventType from, EventType to) const;

  void exportInternalModel(const std::string &filename) const;
  void exportModel(const std::string &filename) const;
  void validate() const;
};

// the method saturates a model by adding edges
// the set of vertices remains unchanged
// for identity one could merge the vertices. but we add equality edges
// this makes tha analysis more easy: saturations can be counted on edges
// all edges of the given model have count 0. The count of a saturated edge is:
// min_[lhs assumption path witness] max{edge in path} + 1
// IMPORTANT: modeling equalities explicit in model requires us to ensure that model is consistent
// wrt. these equalities. Otherwise a naive evaluation of an expression in such a model may me
// wrong. We ensure consistency by the fact that all vertices in the same equivalence class (wrt
// equalities) have the same edges
void saturateModel(Model &model);
