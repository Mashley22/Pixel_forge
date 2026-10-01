module;

#include <ranges>

export module PixelForge.containers:utils.traits;

export namespace pf::adapters {

template <typename Adapter>
concept Adapter_c = requires {
  typename Adapter::ValueType;
  typename Adapter::SizeType;
  typename Adapter::Reference;
  typename Adapter::ConstReference;
  typename Adapter::DifferenceType;
  typename Adapter::Pointer;
  typename Adapter::ConstPointer;
};

template <typename Adapter, typename Range>
concept CompatibleInputRange_c =
    Adapter_c<Adapter> && std::ranges::input_range<Range> &&
    std::ranges::sized_range<Range> &&
    (std::constructible_from<typename Adapter::ValueType,
                             std::ranges::range_value_t<Range>> ||
     std::constructible_from<typename Adapter::ValueType,
                             std::ranges::range_reference_t<Range>>);

template <typename Adapter, typename Range>
concept CompatibleOutputRange_c =
    Adapter_c<Adapter> && std::ranges::output_range<Range, typename Adapter::ValueType>;

}
