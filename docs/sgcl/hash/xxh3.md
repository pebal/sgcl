# sgcl::hash::xxh3_64, sgcl::hash::xxh3_128

```cpp
#include "sgcl/hash/xxh3.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class xxh3_64;    // XXH3, 64 bits, with a seed     value(): uint64_t
    class xxh3_128;   // XXH3, 128 bits, with a seed    value(): array<byte, 16>
}
```

XXH3, the hash of xxHash 0.8: fast on every length, with a seed, in 64 and 128 bits, and stable — its values have been frozen since version 0.8.0 (2020) and are the same on every machine, in every run and in every implementation (`xxhsum -H3` and `-H128`, Go's `github.com/zeebo/xxh3`, Rust's `xxhash-rust`, Python's `xxhash`). It is the hash for what is written down or sent away: the identity of a file's contents, a key of a cache on disk, a checksum in a protocol that names XXH3. Go's standard library has no XXH3; the column Go below is `zeebo/xxh3`.

It is not a defence against keys chosen by an adversary. A seed makes the values differ from the unseeded ones, not unguessable: whoever sees a few values next to their inputs learns enough. A table keyed by what a client sends takes [`maphash`](maphash.md) (its values stay in the process) or, where the hashes may be seen, [`siphash`](siphash.md).

## Rules

- **The seed** is a number, 0 unless given: `xxh3_64(seed)`, `xxh3_64::of(data, seed)`. Two hashers with the same seed agree everywhere; a seed of 0 is XXH3 without one. Past 240 bytes the algorithm reads a secret of 192 bytes made from the seed: a hasher makes it once, when its first stripe goes in, and `of` makes it on its stack for each call over 240 bytes (24 additions).
- **The 128-bit value** is sixteen bytes, the high half first, the canonical form `xxhsum -H128` prints; `value()` and `digest()` are the same thing, as for the 128-bit FNVs.
- **`value()` ends nothing**, as everywhere in the module: the hasher works the result out on a copy of its lanes and goes on.
- **`reset()` keeps the seed**: the hasher is as it was made.
- **Size**: a hasher is eight lanes, a buffer of four stripes (256 bytes) and the seed's secret, about 550 bytes, a plain value: a copy is a branch. `of` makes no hasher.
- There is no hash with a secret of the caller's own (xxHash's `_withSecret` functions): a seed is what the module offers.

## Members

```cpp
static constexpr size_t digest_size = 8;          // 16 for xxh3_128
static constexpr size_t block_size = 64;          // a stripe

xxh3_64() noexcept;                               // seed 0
explicit xxh3_64(uint64_t seed) noexcept;

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
uint64_t value() const noexcept;                  // xxh3_128: array<byte, 16>, the high half first
array<byte, 8> digest() const noexcept;      // the value, the most significant byte first
void reset() noexcept;                            // as new, with the seed it was made with

static uint64_t of(/* bytes or text */) noexcept;                   // seed 0
static uint64_t of(/* bytes or text */, uint64_t seed) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

`xxh3_128` has the same members, its value the digest.

## Paths

Inputs up to 240 bytes take one of six paths by length, a multiplication or a few; longer ones go through eight lanes of 64 bits, a stripe of 64 bytes at a time. On arm64 the lanes are four NEON vectors: the compiler keeps the loop's eight products scalar, and the vectors are markedly faster (measured side by side with the plain loop). Elsewhere, and with `SGCL_HASH_PORTABLE` defined, the loop is the plain one; the tests hold the two against each other and both against the oracles. As for the CRCs, the path is chosen per file from the target's flags, so a program is built for one target throughout.

## Example

```cpp
#include "sgcl/encoding/hex.h"
#include "sgcl/hash/xxh3.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

using namespace sgcl;

int main() {
    io::stdout.write(txt::format("{:016x}\n", hash::xxh3_64::of("")));
    io::stdout.write(txt::format("{:016x}\n", hash::xxh3_64::of("hello")));
    io::stdout.write(txt::format("{:016x}\n", hash::xxh3_64::of("hello", 42)));

    // in pieces, the same value
    hash::xxh3_64 h(42);
    h.update("hel");
    h.update("lo");
    io::stdout.write(txt::format("{:016x}\n", h.value()));

    // 128 bits: sixteen bytes, as xxhsum -H128 prints them
    io::stdout.write(encoding::hex::encode(hash::xxh3_128::of("hello")) + "\n");
}
```

Output:

```text
2d06800538d394c2
9555e8555c62dcfd
bafa072f07db7937
bafa072f07db7937
b5e9c1ad071b3e7fc779cfaa5e523818
```

## See also

[The module](README.md); [`maphash`](maphash.md) for a table in memory, [`siphash`](siphash.md) for keys from an adversary; [`mixin::hasher`](hasher.md), the shape every hasher shares.
