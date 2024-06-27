class OccurrenceCounter;
typedef const OccurrenceCounter *CanonicalOccurrenceCounter;
typedef int CounterType;

class OccurrenceCounter {
 private:
  static CanonicalOccurrenceCounter newOccurrenceCounter(CounterType counter,
                                                         CanonicalOccurrenceCounter left,
                                                         CanonicalOccurrenceCounter right);
  bool validate() const;  // TODO:

 public:
  OccurrenceCounter(CounterType counter, CanonicalOccurrenceCounter left,
                    CanonicalOccurrenceCounter right);
  static CanonicalOccurrenceCounter newCounter(CounterType counter);
  static CanonicalOccurrenceCounter newOccurence(CanonicalOccurrenceCounter left);
  static CanonicalOccurrenceCounter newOccurence(CanonicalOccurrenceCounter left,
                                                 CanonicalOccurrenceCounter right);

  // Due to canonicalization, moving or copying is not allowed
  OccurrenceCounter(const OccurrenceCounter &other) = delete;
  OccurrenceCounter(const OccurrenceCounter &&other) = delete;

  bool operator==(const OccurrenceCounter &other) const;

  const CounterType counter;
  const CanonicalOccurrenceCounter left;   // is set iff operation unary/binary
  const CanonicalOccurrenceCounter right;  // is set iff operation binary
};

template <>
struct std::hash<OccurrenceCounter> {
  std::size_t operator()(const OccurrenceCounter &counter) const noexcept {
    const size_t leftHash = hash<CanonicalOccurrenceCounter>()(counter.left);
    const size_t rightHash = hash<CanonicalOccurrenceCounter>()(counter.right);
    const size_t counterHash = hash<CounterType>()(counter.counter);

    return (leftHash ^ (rightHash << 1) >> 1) + 31 * counterHash;
  }
};