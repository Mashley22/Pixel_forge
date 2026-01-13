if command -v ninja &> /dev/null; then
  echo "Found ninja!"
else
  echo "Need ninja for the c++ modules stuff!" >&2
  exit 1
fi

BASE_CMAKE_BUILD_COMMAND="cmake ../.. -G Ninja"
