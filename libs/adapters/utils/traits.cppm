module;

#include <ranges>

export module PixelForge.adapters.utils.traits;

export namespace pf::adapters {
  
template<typename Adapter>
concept Adapter_c = requires {
  typename Adapter::value_type;
  typename Adapter::size_type;
  typename Adapter::reference;
  typename Adapter::const_reference;
  typename Adapter::difference_type;
  typename Adapter::pointer;
  typename Adapter::const_pointer;
};

template<typename Adapter, typename Range>
concept CompatibleInputRange_c = 
  Adapter_c<Adapter> &&
  std::ranges::input_range<Range> &&
requires {
  std::convertible_to<std::ranges::range_value_t<Range>, typename Adapter::value_type> ||
  std::convertible_to<std::ranges::range_reference_t<Range>, typename Adapter::value_type>;
};

template<typename Adapter, typename Range>
concept CompatibleOutputRange_c = 
  Adapter_c<Adapter> &&
  std::ranges::output_range<Range, typename Adapter::value_type>;

}
