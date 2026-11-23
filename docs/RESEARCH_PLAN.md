# Research plan

## Experimental directions

### A. Pure neural memorization

Train a network from byte positions to byte values. Quantize and store weights,
then reconstruct by inference. Measure the resulting model payload and require
byte-for-byte equality before considering compression performance.

### B. Neural memorization with residual

Train a model to approximate the input and encode residual information so the
decoder can reconstruct the original exactly. Count weights, residual stream,
format metadata, and every other decoding requirement in total size.

### C. Neural probability prediction

Train a predictor for byte probability distributions and encode the original
with a deterministic entropy coder. Define integer probability quantization,
normalization, and tie-breaking rules to keep decoding reproducible.

All experiments must use the same datasets, train/validation protocol,
measurement rules, and resource budgets. Compare against Zstd, Brotli, and LZMA
using documented configurations and versions.

## Metrics

- Total compressed bits, including model, residual, metadata, and framing
- Compression ratio and bits per input byte
- Training, compression, and decompression time
- Peak memory during each phase
- Reconstruction correctness (exact byte equality)
- Cross-platform decoding consistency and deterministic output

The repository currently contains no training, entropy coding, file format
serialization, or compression implementation. These experiments are plans,
not measured results.
