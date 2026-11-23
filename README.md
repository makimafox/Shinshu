# Neural Memorization Compression

An experimental C++20 project to test whether a neural network can memorize a
binary file compactly enough that quantized weights, optionally with a residual
stream, beat conventional lossless compression while reconstructing every byte
exactly. This is a research hypothesis; no compression results are claimed.

## Architecture

The project contains deterministic synthetic data, integrity gates, and an
identity-only round-trip pipeline. Real codec and neural backends remain
unimplemented. See [Architecture](docs/ARCHITECTURE.md),
[Data Pipeline](docs/DATA_PIPELINE.md),
[Generated Corpus](docs/GENERATED_CORPUS.md), and
[Compression Test Contract](docs/COMPRESSION_TEST_CONTRACT.md).

## Prerequisites

- CMake 3.25 or newer
- C++20 compiler (Clang recommended; GCC and MSVC configurations are supported)
- Ninja for the supplied presets
- vcpkg in manifest mode for GoogleTest and nlohmann-json (`VCPKG_ROOT` points
  to its checkout)
- clang-format and clang-tidy for the corresponding quality checks

The project does not install LibTorch, Eigen, CUDA, ONNX Runtime, or Google
Benchmark. GoogleTest supports unit tests; nlohmann-json handles the versioned
manifest and report files.

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

Without vcpkg, provide installed GoogleTest and nlohmann-json headers/CMake
packages. The build can discover nlohmann-json from its header path; tests are
enabled when GoogleTest 1.11 or newer is discoverable.

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
shinshu data generate --all
shinshu data generate --profile pseudorandom --size 65536
shinshu data validate
shinshu data list
shinshu data summary
shinshu test roundtrip --codec identity
shinshu test report
```

The `data` commands generate and validate synthetic data in the current working
directory. Identity round trips validate pipeline behavior and are marked
`INFRASTRUCTURE_ONLY`. gzip, xz, zstd, and neural backends are reported as not
implemented. No compression performance is claimed.

## Status and phases

Implemented: C++20 data profiles, edge-case files, deterministic manifest,
SHA-256 validation, corpus gates, identity round trips, JSON/CSV reports,
CLI argument handling, GoogleTest coverage, sanitizer and lint configuration.

Planned: implement safe real codec adapters and malformed-stream tests, then
define neural experiment formats and compare experiments A, B, and C on this
generated corpus. The identity adapter is test infrastructure and provides no
compression.
