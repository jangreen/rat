#include "ReasonAnnotation.h"

std::size_t std::hash<Reasons>::operator()(const Reasons &reasons) const noexcept {
  size_t seed = 0;
  for (const auto reason : reasons) {
    if (std::holds_alternative<CanonicalSet>(reason)) {
      const auto set = std::get<CanonicalSet>(reason);
      boost::hash_combine(seed, std::hash<CanonicalSet>()(set));
    } else {
      const auto relation = std::get<CanonicalRelation>(reason);
      boost::hash_combine(seed, std::hash<CanonicalRelation>()(relation));
    }
  }
  return seed;
}
