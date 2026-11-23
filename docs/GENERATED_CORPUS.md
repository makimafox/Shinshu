# Generated corpus definitions

Generator version: **1.0.0**. All primary files are exactly 4,096, 16,384,
65,536, or 262,144 bytes. Profiles use ASCII-defined bytes and fixed rules.
IDs use profile codes and decimal sizes; they are constructed directly, never
from `std::hash`.

| ID | Profile | Rule | Seed |
|---|---|---|---|
| G01 | `zero` | Every byte is `0x00`. | 0 |
| G02 | `repeat_abc` | Repeat the four bytes `a`, `b`, `c`, LF; truncate at the requested size. | 0 |
| G03 | `sequential` | Byte at offset `i` is `i mod 256`. | 0 |
| G04 | `pseudorandom` | SplitMix64 words emitted least significant byte first. | `0x5348494E53485531` |
| G05 | `generated_text` | Cycle through the fixed 24-word vocabulary below; separate words with spaces and put LF after each eighth token. | 0 |
| G06 | `alternating` | `0x00`, `0xFF`, starting with `0x00`. | 0 |
| G07 | `sparse_random` | Start with zeroes; for `size / 64` entries, set `floor(i * size / count)` to `1 + next() mod 255`. | `0x5350415253450001` |
| G08 | `mixed_entropy` | Alternate 1,024-byte blocks: repeat `abc`+LF in even blocks; SplitMix64 bytes in odd blocks using seed XOR block index. | `0x4D49584544000100` |

SplitMix64-v1 has 64-bit unsigned wraparound. Each `next()` adds
`0x9E3779B97F4A7C15` to state, then applies, in order:

1. `z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9`
2. `z = (z ^ (z >> 27)) * 0x94D049BB133111EB`
3. `z = z ^ (z >> 31)`

Every operation is on `uint64_t`. A generated word contributes bytes at bit
shifts 0, 8, 16, …, 56. The pseudorandom profile continues consuming words
until the file is full. G08 starts a new SplitMix64 stream per odd block with
`seed XOR zero-based block index`.

The G05 vocabulary, in order, is:
`amber birch cinder delta ember fable garden harbor island jasmine kernel
lantern meadow nectar ocean petal quartz ribbon silver timber umber velvet
willow zephyr`.

Edge cases are also deterministic:

- `E00-empty-v1`: zero bytes.
- `E01-single-byte-v1`: one byte, `0xA5`.
- `E02-all-byte-values-v1`: values 0 through 255 once each.
- `E03-size-N-v1`: sizes 255, 256, 257, 4,095, 4,096, and 4,097; byte `i`
  is `i mod 256`.

This produces 41 files total. The manifest records the profile, generator
version, seed, size, relative path, SHA-256, parameters, and expected
properties. Deterministic pseudorandom files are controlled test cases; their
results do not establish compression performance on general data.
