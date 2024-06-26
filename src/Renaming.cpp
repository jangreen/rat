#include "Renaming.h"

Renaming::Renaming(std::vector<int> from) : from(from) {
  std::vector<int> to;
  to.reserve(from.size());

  for (size_t i = 0; i < from.size(); i++) {
    to.push_back(i);
  }
  this->to = to;
}

Renaming::Renaming(int from, int to) : from({from}), to({to}) {}
