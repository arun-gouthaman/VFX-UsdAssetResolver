# Clear current session log 
clear
# Source environment (Uncomment lines starting with "export" if you current env does not have these defined.)
# export HFS=/opt/<InsertHoudiniVersion>
# Define Resolver > Has to be one of 'fileResolver'/'pythonResolver'/'cachedResolver'/'httpResolver'
# export RESOLVER_NAME=cachedResolver
# Clear existing build data and invoke cmake
rm -R build
rm -R dist
set -e # Exit on error
cmake . -B build
cmake --build build --clean-first              # make clean all
cmake --install build                          # make install
ctest -VV --test-dir build