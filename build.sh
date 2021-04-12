
CONFIG=${1-Release}
TARGET=${2-all}
mkdir -p build
cd build
echo "Building target '${TARGET}' in ${CONFIG} configuration"
cmake -DCMAKE_BUILD_TYPE=$CONFIG ..
cmake --build . --config $CONFIG --target $TARGET -j 8