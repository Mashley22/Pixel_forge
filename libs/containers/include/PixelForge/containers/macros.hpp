#ifndef PF_CONTAINERS_MACROS_HPP
#define PF_CONTAINERS_MACROS_HPP

#define PF_CONTAINERS_INHERIT_TRAITS(traits)     \
  using ValueType = traits::ValueType;           \
  using SizeType = traits::SizeType;             \
  using DifferenceType = traits::DifferenceType; \
  using Reference = traits::Reference;           \
  using ConstReference = traits::ConstReference; \
  using Pointer = traits::Pointer;               \
  using StorageType = traits::StorageType;       \
  using ConstPointer = traits::ConstPointer // semicolon after!!

#endif /* PF_CONTAINERS_MACROS_HPP */
