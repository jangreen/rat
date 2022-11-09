cmake -S . -B build -Wno-dev
cmake --build build
echo "----------- Compilation done -----------"
./build/CatInfer