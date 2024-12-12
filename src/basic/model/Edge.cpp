#include "Edge.h"

#include "../Set.h"

Edge::Edge(const EventType from, const EventType to, const CanonicalRelation reason)
    : _from(from), _to(to), _reason(reason) {}

Edge::Edge(const EventType from, const EventType to)
    : _from(from),
      _to(to),
      _reason(Relation::cartesianProduct(Set::newEvent(from), Set::newEvent(to))) {}

EventType Edge::from() const { return _from; }

EventType Edge::to() const { return _to; }

CanonicalRelation Edge::reason() const { return _reason; }

void Edge::resetReason() { _reason = nullptr; }

bool Edge::updateReason(const CanonicalRelation newReason) {
  assert(newReason != nullptr);
  if (newReason->isSmallerReason(_reason)) {
    _reason = newReason;
    return true;
  }
  return false;
}

bool Edge::operator==(const Edge& other) const { return _from == other._from && _to == other._to; }

bool Edge::operator<(const Edge& other) const {
  return std::tie(_from, _to) < std::tie(other._from, other._to);
}

Edge Edge::converse() const { return {_to, _from, _reason->converse()}; }

std::optional<Edge> Edge::compose(const Edge& other) const {
  if (to() != other.from()) {
    return std::nullopt;
  }
  return Edge(from(), other.to(), _reason->composeWith(other.reason()));
}

std::optional<Edge> Edge::intersect(const Edge& other) const {
  if (from() == other.from() && to() == other.to()) {
    return Edge(from(), to(), _reason->intersectWith(other.reason()));
  }
  return std::nullopt;
}

std::size_t std::hash<Edge>::operator()(const Edge& edge) const noexcept {
  return hash<EventType>()(edge.from()) | (hash<EventType>()(edge.to()) << 16);
}
