SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo $SCRIPT_DIR
source $SCRIPT_DIR/../../build_utils.sh

eval cmake ../../../ -G Ninja -DPIXELFORGE_AGGRESSIVE_OPTIMISATIONS=ON "$@" -DPIXELFORGE_TEST=ON
