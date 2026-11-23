# Architecture

The initial executable is deliberately small. `shinshu_core` owns version metadata
and command parsing; `shinshu_cli` is the process boundary and reports unsupported
operations explicitly. The `shinshu::entropy`, `shinshu::format`, and `shinshu::neural`
interface targets export public headers without adding speculative runtime
code or dependencies.

Public headers define framework-independent contracts. Compressor and
decompressor implementations will coordinate the format, model, and entropy
components. The model boundary exposes deterministic position-to-byte
inference; training remains separate from the decoder. Binary format code must
define fixed endianness, checked lengths, versioning, and validation before
accepting untrusted streams.

No neural framework, GPU runtime, or numerical library is part of the initial
build. LibTorch C++ and Eigen can be evaluated later behind the neural target.
Exact reconstruction is mandatory. Floating-point training and inference may
vary across compiler, hardware, and library versions, so future formats must
record quantized weights and use deterministic integer probability handling
for entropy coding.
