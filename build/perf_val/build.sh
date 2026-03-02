SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo $SCRIPT_DIR
source $SCRIPT_DIR/../build_utils.sh

eval cmake ../../ -G Ninja -DCMAKE_BUILD_TYPE=Release -DPIXELFORGE_AGGRESSIVE_OPTIMISATIONS=ON "$@" -DPIXELFORGE_PERF_VAL=ON
