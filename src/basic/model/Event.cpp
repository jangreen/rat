#include "Event.h"

Event::Event(const EventType event) : _event(event), _reason(Set::newEvent(event)) {}

Event::Event(const EventType event, const CanonicalSet reason) : _event(event), _reason(reason) {}

EventType Event::event() const { return _event; }

CanonicalSet Event::reason() const { return _reason; }

bool Event::updateReason(const CanonicalSet newReason) {
  assert(newReason != nullptr);
  if (newReason->isSmallerReason(_reason)) {
    _reason = newReason;
    return true;
  }
  return false;
}

bool Event::operator==(const Event& other) const { return _event == other._event; }

bool Event::operator<(const Event& other) const { return _event < other._event; }

std::optional<Event> Event::intersect(const Event& other) const {
  if (event() == other.event()) {
    return Event(event(), _reason->intersectWith(other.reason()));
  }
  return std::nullopt;
}

std::optional<Event> Event::image(const Edge& edge) const {
  if (event() == edge.from()) {
    return Event(edge.to(), _reason->imageWith(edge.reason()));
  }
  return std::nullopt;
}

std::optional<Event> Event::domain(const Edge& edge) const {
  if (event() == edge.to()) {
    return Event(edge.from(), _reason->domainWith(edge.reason()));
  }
  return std::nullopt;
}

std::size_t std::hash<Event>::operator()(const Event& event) const noexcept {
  return std::hash<EventType>()(event.event());
}
