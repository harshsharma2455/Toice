#!/bin/bash
# toice-build.sh

echo "Building Toice..."

# 1. Build whisper.cpp
cd "$(dirname "$0")/cpp_native/whisper.cpp"
cmake -B build -DGGML_OPENMP=ON
cmake --build build --config Release -j$(nproc)
cd ../..

# 2. Build Toice
mkdir -p "cpp_native/build"
cd "cpp_native/build"
cmake ..
make -j$(nproc)

# 3. Copy Models
cd ../..
mkdir -p "models"
if [ ! -f "models/ggml-base.en.bin" ]; then
    echo "Downloading model..."
    curl -L https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin -o "models/ggml-base.en.bin"
fi
mkdir -p "cpp_native/build/models"
cp -v "models/ggml-base.en.bin" "cpp_native/build/models/"

echo "Build Complete! Binary is at cpp_native/build/Toice"
