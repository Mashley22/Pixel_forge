if command -v ninja &> /dev/null; then
  echo "Found ninja!"
else
  echo "Need ninja for the c++ modules stuff!" >&2
  exit 1
fi

CLANG_BUILD_ARGS="-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_EXPORT_COMPILE_COMMANDS=y"
