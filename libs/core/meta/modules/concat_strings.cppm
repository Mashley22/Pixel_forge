module;

#include <string_view>

export module PixelForge.core.meta.concatStrings;

namespace pf {

namespace meta {

// From: https://stackoverflow.com/questions/38955940/how-to-concatenate-static-strings-at-compile-time/62823211#62823211
// User: nitronoid - How to concatenate static strings at compile time
// Note that most llms will give a crappy version repat of this when asked for something similar
export
template<std::string_view const&... V_strs>
struct ConcatStrings {
private:

[[nodiscard]]
static consteval
std::size_t
total_len(void) { // including null terminator
  return (V_strs.size() + ...) + 1;
}

[[nodiscard]]
static consteval 
std::array<char, total_len()>
impl(void) {
  std::array<char, total_len()> retVal{}; // for null terminator
  std::size_t pos = 0;

  auto copy = [&](std::string_view str) {
      std::copy(str.begin(), str.end(), retVal.begin() + pos);
      pos += str.size();
  };
  
  (copy(V_strs), ...);

  retVal.back() = '\0';
  return retVal;
}

public:

static constexpr
std::array<char, total_len()> arr = impl();

[[nodiscard]]
static consteval // constevals unneccesary but oh well
std::string_view sv(void) { return {arr.data(), arr.size() - 1}; }

[[nodiscard]]
static consteval
const char* c_str(void) noexcept { return arr.data(); }

};

}

}
