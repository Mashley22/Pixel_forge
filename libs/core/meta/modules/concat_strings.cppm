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

static constexpr auto impl(void) noexcept {
  constexpr std::size_t total_len = (V_strs.size() + ...);
  std::array<char, total_len + 1> res{}; // for null terminator
  std::size_t pos = 0;

  auto copy = [&](std::string_view str) {
      std::copy(str.begin(), str.end(), res.begin() + pos);
      pos += str.size();
  };
  
  (copy(V_strs), ...);
  res[total_len] = '\0';
  return res;
}

public:

static constexpr
auto arr = impl();

static constexpr
std::string_view sv() { return {arr.data(), arr.size() - 1}; }

static constexpr
const char* c_str(void) noexcept { return arr.data(); }

};

}

}
