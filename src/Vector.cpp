#include <boost/functional/hash.hpp>
#include <vector>

namespace std {
template <typename T>
struct hash<std::vector<T>> {
  typedef std::vector<T> argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const &in) const {
    size_t size = in.size();
    size_t seed = 0;
    for (size_t i = 0; i < size; i++)
      // Combine the hash of the current std::vector with the hashes of the previous ones
      boost::hash_combine(seed, in[i]);
    return seed;
  }
};
}  // namespace std

// TODO not used
