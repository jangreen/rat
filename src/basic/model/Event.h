#pragma once
#include "EventType.h"
#include "../Set.h"
#include "../model/Edge.h"

class Event {
  EventType _event;
  CanonicalSet _reason;

 public:
  explicit Event(EventType event);
  Event(EventType event, CanonicalSet reason);

  [[nodiscard]] EventType event() const;
  [[nodiscard]] CanonicalSet reason() const;
  [[nodiscard]] bool updateReason(CanonicalSet newReason);
  [[nodiscard]] bool operator==(const Event &other) const;
  [[nodiscard]] bool operator<(const Event &other) const;
  [[nodiscard]] std::optional<Event> intersect(const Event &other) const;
  [[nodiscard]] std::optional<Event> image(const Edge &edge) const;
  [[nodiscard]] std::optional<Event> domain(const Edge &edge) const;
};

template <>
struct std::hash<Event> {
  std::size_t operator()(const Event &event) const noexcept;
};

