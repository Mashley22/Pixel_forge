#ifndef PF_ADAPTERS_MACROS_HPP
#define PF_ADAPTERS_MACROS_HPP

#define PF_ADAPTERS_INHERIT_TRAITS(traits) \
  using value_type = traits::value_type; \
  using size_type = traits::size_type; \
  using difference_type = traits::difference_type; \
  using reference = traits::reference; \
  using const_reference = traits::const_reference; \
  using pointer = traits::pointer; \
  using const_pointer = traits::const_pointer // semicolon after!!

#endif /* PF_ADAPTERS_MACROS_HPP */
