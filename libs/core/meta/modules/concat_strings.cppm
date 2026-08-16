module;

#include <string_view>

export module PixelForge.core:meta.concatStrings;

namespace pf {

namespace meta {

// Basically taken from here
// From:
// https://stackoverflow.com/questions/38955940/how-to-concatenate-static-strings-at-compile-time/62823211#62823211
// User: nitronoid - How to concatenate static strings at compile time
// Note that most llms will give a crappy version repat of this when asked for something similar
export template <std::string_view const&... V_strs>
struct ConcatStrings {
private:
  [[nodiscard]]
  static consteval std::size_t
  total_len() { // including null terminator
    return (V_strs.size() + ...) + 1;
  }

  [[nodiscard]]
  static consteval std::array<char, total_len()>
  impl() {
    std::array<char, total_len()> retVal{};
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
  static constexpr std::array<char, total_len()> arr = impl();

  [[nodiscard]]
  static consteval std::string_view
  sv() {
    return {arr.data(), arr.size() - 1};
  }

  [[nodiscard]]
  static consteval std::string_view
  str() {
    return {arr.data(), arr.size() - 1};
  }

  [[nodiscard]]
  static consteval const char*
  c_str() noexcept {
    return arr.data();
  }
};

}

}
