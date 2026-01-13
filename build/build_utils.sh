if command -v ninja &> /dev/null; then
  echo "Found ninja!"
else
  echo "Need ninja for the c++ modules stuff!" >&2
  exit 1
fi

PENDANTIC_COMPILE_OPTIONS="\
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  -Wshadow \
  -Wconversion \
  -Wsign-conversion \
  -Wcast-align \
  -Wcast-qual \
  -Wformat=2 \
  -Wundef \
  -Wnull-dereference \
  -Wdouble-promotion \
  -Wimplicit-fallthrough \
  -Wmissing-declarations \
  -Wredundant-decls \
  -Wstrict-overflow=5 \
  -Wswitch-enum \
  -Wunreachable-code"

CMAKE_PEDANTIC_COMPILE_OPTIONS="-DCMAKE_CXX_FLAGS=${NOT_MSVC_PENDANTIC_COMPILE_OPTIONS}"

BASE_CMAKE_BUILD_COMMAND="cmake ../.. -G Ninja"
