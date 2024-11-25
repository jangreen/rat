#pragma once

// currently it is not possible to pass a concept as a template parameter
// we would like to write something like this: range_of<Literal>, range_of<rane_of<Literal>>, ...
// template <typename RangeType, template <typename> concept ValueConcept>
// concept range_of =
//     std::ranges::range<RangeType> && ValueConcept<std::ranges::range_value_t<RangeType>>;
// workaround:
template <class RangeType, class RangeValueType>
concept range_of = std::ranges::range<RangeType> &&
                   std::same_as<RangeValueType, std::ranges::range_value_t<RangeType>>;
template <class RangeType, class RangeValueType>
concept range_of_range_of = std::ranges::range<RangeType> &&
                            range_of<std::ranges::range_value_t<RangeType>, RangeValueType>;