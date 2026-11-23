# Neural Memorization Compression

An experimental C++20 project to test whether a neural network can memorize a
binary file compactly enough that quantized weights, optionally with a residual
stream, beat conventional lossless compression while reconstructing every byte
exactly. This is a research hypothesis; no compression results are claimed.

## Architecture

The CLI and small core are buildable today. Public interfaces outline future
compressor, decompressor, model, entropy coder, and file-format boundaries.
See [Architecture](docs/ARCHITECTURE.md) and [Research Plan](docs/RESEARCH_PLAN.md).

## Prerequisites

- CMake 3.25 or newer
- C++20 compiler (Clang recommended; GCC and MSVC configurations are supported)
- Ninja for the supplied presets
- vcpkg in manifest mode for GoogleTest (`VCPKG_ROOT` points to its checkout)
- clang-format and clang-tidy for the corresponding quality checks

The initial project does not install LibTorch, Eigen, CUDA, ONNX Runtime, or
Google Benchmark. GoogleTest is its only declared package dependency.

## Configure and build

```sh
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset debug
ctest --preset debug
```

For optimized builds, substitute `release` for `debug`. The `debug-sanitizers`
preset enables AddressSanitizer and UndefinedBehaviorSanitizer with Clang or
GCC. CMake exports `compile_commands.json` into each build directory.

On systems without vcpkg, configure with tests' dependency-free CLI contracts:

```sh
cmake -S . -B build/local -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/local
ctest --test-dir build/local --output-on-failure
```

The vcpkg baseline is pinned in `vcpkg.json` and
`vcpkg-configuration.json`. Benchmark support is opt-in and will fail clearly
until real benchmarks are added. clang-format can check C++ files with
`clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')`; clang-tidy
can use each build's compile database.

## CLI

```sh
shinshu --help
shinshu --version
shinshu compress input.bin -o output.shinshu
shinshu decompress input.shinshu -o restored.bin
shinshu verify output.shinshu
shinshu benchmark input.bin
```

Help and version work. The other commands validate their argument shapes, then
return a nonzero `not implemented` error. They do not read or write files.

## Status and phases

Implemented: C++20 CMake project, CLI help/version/argument validation,
dependency-free CTest contracts, optional GoogleTest API tests, format and
research interface declarations, sanitizer and lint configuration.

Planned: define a versioned binary format; implement safe conventional codec
baselines and deterministic round-trip tests; establish a dataset and resource
budget protocol; then implement and compare experiments A, B, and C. The
interface declarations do not imply that compression or neural inference is
functional.
