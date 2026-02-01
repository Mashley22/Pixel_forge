module;

#include <ranges>

export module PixelForge.views.utils.traits;

export namespace pf::views {
  
template<typename View>
concept View_c = requires {
  typename View::value_type;
  typename View::size_type;
  typename View::reference;
  typename View::const_reference;
  typename View::difference_type;
  typename View::pointer;
  typename View::const_pointer;
};

template<typename View, typename Range>
concept CompatibleInputRange_c = 
  View_c<View> &&
  std::ranges::input_range<Range> &&
requires {
  std::convertible_to<std::ranges::range_value_t<Range>, typename View::value_type> ||
  std::convertible_to<std::ranges::range_reference_t<Range>, typename View::value_type>;
};

template<typename View, typename Range>
concept CompatibleOutputRange_c = 
  View_c<View> &&
  std::ranges::output_range<Range, typename View::value_type>;

}
