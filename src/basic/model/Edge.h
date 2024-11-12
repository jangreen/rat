#pragma once

#include <optional>

#include "../Relation.h"
#include "EventType.h"

class Edge {
  EventType _from;
  EventType _to;
  CanonicalRelation _reason;

 public:
  Edge(EventType from, EventType to);
  Edge(EventType from, EventType to, CanonicalRelation reason);

  [[nodiscard]] EventType from() const;
  [[nodiscard]] EventType to() const;
  [[nodiscard]] CanonicalRelation reason() const;
  [[nodiscard]] bool updateReason(CanonicalRelation newReason);
  [[nodiscard]] bool operator==(const Edge &other) const;
  [[nodiscard]] bool operator<(const Edge &other) const;

  [[nodiscard]] Edge converse() const;
  [[nodiscard]] std::optional<Edge> compose(const Edge &other) const;
  [[nodiscard]] std::optional<Edge> intersect(const Edge &other) const;
};

template <>
struct std::hash<Edge> {
  std::size_t operator()(const Edge &edge) const noexcept;
};
