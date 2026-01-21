module;

#include <concepts>
#include <string_view>

export module PixelForge.core.meta.intToStr;

import PixelForge.core.utils.charEncoding;

namespace pf {

namespace meta {

// can also use the IntToSv, this is just a very natural stopping off point in the implementation
// might make this a private impl ...
export
template<
  std::unsigned_integral T,
  T T_val,
  std::size_t T_base = 10,
  const std::string_view& T_digitSet = digitSetUpper
>
struct UintToStr {
private:

[[nodiscard]]
static consteval
std::size_t
digits(void) {
  std::size_t digitCount = 1;
  T val = T_val;
  
  while(val > T_base) {
    digitCount++;
  }

  return digitCount;
}

[[nodiscard]]
static consteval
std::size_t
len(void) {
  return digits() + 1;
}

[[nodiscard]]
static consteval
T
maxBaseScale(void) {
  T val = T_base;
  while (static_cast<T>(val * T_base) < T_val) {
    val *= T_base;
  }

  return val;
}

[[nodiscard]]
static consteval
std::array<char, len()>
impl(void) {
  std::array<char, len()> retVal{};
  T val = T_val;
  T scale = maxBaseScale();
  
  for (std::size_t i = 0; i < digits(); i++) {
    val %= scale;
    retVal[i] = T_digitSet[val];
    scale /= T_base;
    (void)scale;
  }

  retVal.back() = '\0';

  return retVal;
}

public:

static constexpr
std::array<char, len()>
arr = impl();

[[nodiscard]]
static consteval
std::string_view
sv(void) { return {arr.data(), arr.size() - 1}; }

[[nodiscard]]
static consteval
const char *
c_str(void) { return arr.data(); }

[[nodiscard]]
static consteval
std::string_view
str(void) { return {arr.data(), arr.size() - 1}; }

};

export
template<
  std::signed_integral T,
  T T_val,
  std::size_t T_base = 10,
  const std::string_view& T_digitSet = digitSetUpper
>
struct IntToStr {
private:

using unsigned_t = std::make_unsigned_t<T>;

static constexpr auto UintArr = UintToStr<unsigned_t, static_cast<unsigned_t>(T_val), T_base, T_digitSet>::arr;

[[nodiscard]]
static consteval
std::size_t len(void) {
  if constexpr (T_val > 0) {
    return UintArr.size();
  }
  return UintArr.size() + 1;
}

[[nodiscard]]
static consteval
std::array<char, len()>
impl(void) {
  std::array<char, len()> retVal;
  if constexpr (T_val > 0) {
    std::copy(UintArr.begin(), UintArr.end(), retVal.begin());
    return retVal;
  }
  
  retVal[0] = '-';
  std::copy(UintArr.begin(), UintArr.end(), retVal.begin() + 1);
  return retVal;
}

public:

static constexpr
auto
arr = impl();

[[nodiscard]]
static consteval
std::string_view
sv(void) { return {arr.data(), arr.size() - 1}; }

[[nodiscard]]
static consteval
const char *
c_str(void) { return arr.data(); }

};

}

}
