# Compression test contract

## Adapter boundary

`CodecAdapter` is the common interface for `gzip`, `xz`, `zstd`, `neural`,
`neural-residual`, and `neural-probability`. Only `identity` is implemented.
Unavailable adapters return `not_implemented`; they never return a successful
empty artifact or simulated size. `malformed_input` is the required result for
future decoders given malformed streams or truncated input.

The identity adapter is marked `INFRASTRUCTURE_ONLY` and
`comparison_eligible=false`. Its measured output size equals the input size,
but it is excluded from codec performance comparisons.

## Round-trip sequence

For each manifest entry the runner:

1. Reads the original file and checks its size and SHA-256.
2. Calls adapter compression and records the returned artifact size only on
   success.
3. Decompresses that artifact.
4. Compares lengths and every byte, counting missing, extra, or differing
   bytes and recording the first differing offset.
5. Checks SHA-256 equality of original and reconstructed bytes.

Unavailable measurements are JSON `null` and blank CSV fields. Peak memory is
currently not measured and remains null. Compression ratio means
`input_bytes / compressed_bytes`; bits per byte means
`compressed_bytes * 8 / input_bytes`. Both are null when undefined, including
the empty identity input.

## Reports

`shinshu test roundtrip --codec identity` writes
`data/manifests/roundtrip_records.json`, `roundtrip_report.json`, and
`roundtrip_report.csv`. `shinshu test report` rebuilds the JSON and CSV summary
from the saved records. These compression test reports are distinct from the
validation-only `validation_report.json`.

Records include run ID, Git commit at configure time, build configuration,
compiler, OS, architecture, dataset ID/profile, input length and hash, codec
name/version, compressed length, ratio, bits per byte, compression and
decompression durations, peak memory, mismatch count/offset, result, and
failure reason. Durations are measured with `steady_clock`; unavailable values
are null. No compression performance is claimed until a real adapter is
integrated.

Future codec tests should exercise empty input, all binary values, valid round
trips, corrupt headers/payloads, truncated headers/payloads, and resource-limit
failures. A malformed or truncated stream must not be reported as a valid
round trip.
