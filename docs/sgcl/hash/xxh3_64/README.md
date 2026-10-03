[sgcl](../../README.md) › [hash](../README.md)

# sgcl::hash::xxh3_64

```cpp
#include "sgcl/hash/xxh3.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class xxh3_64;  // XXH3, 64 bits, with a seed
}
```

`sgcl::hash::xxh3_64` is XXH3 of 64 bits, the hash of xxHash 0.8, with a seed. Its values have been frozen since
version 0.8.0 (2020) and are the same on every machine, in every run and in every implementation: `xxhsum -H3`,
Go's `github.com/zeebo/xxh3`, Rust's `xxhash-rust`, Python's `xxhash`. It is the hash for what is written down or
sent away: the identity of a file's contents, a key of a cache on disk, a checksum in a protocol that names XXH3.
[xxh3_128](../xxh3_128/README.md) is the same with 128 bits, where 64 collide too often. Go's standard library has no XXH3;
the Go column below is `zeebo/xxh3`.

It is not a defence against keys chosen by an adversary. A seed makes the values differ from the unseeded ones, not
unguessable: whoever sees a few values next to their inputs learns enough. A table keyed by what a client sends
takes [maphash](../maphash/README.md), whose values stay in the process, or, where the hashes may be seen,
[siphash](../siphash/README.md).

## Rules

- **The seed** is a number, 0 unless given: `xxh3_64(seed)`, `xxh3_64::of(data, seed)`. Two hashers with the same
  seed agree everywhere; a seed of 0 is XXH3 without one. There is no hash with a secret of the caller's own
  (xxHash's `_withSecret` functions): a seed is what the module offers.
- **`value()` ends nothing**: the hasher works the result out on a copy of its lanes and goes on.
- **`digest()` is big-endian**, `value()` the most significant byte first, as Go's `Sum` writes it.
- **`reset()` keeps the seed**: the hasher is as it was made.
- **552 bytes**: eight lanes, a buffer of four stripes (256 bytes) and the secret of the seed (192 bytes). A plain
  value, trivially copyable, nothing for the collector, so a copy is a branch. `of` makes no hasher.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `xxh3.Hash(b)` | `xxh3_64::of(b)` |
| `xxh3.HashSeed(b, seed)` | `xxh3_64::of(b, seed)` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `64` | a stripe, the bytes the algorithm takes at a time on a long input, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xxh3_64.md) | a hasher of no bytes yet, with a seed or without |

#### Modifiers

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes |
| [reset](reset.md) | puts the hasher back as it was made, with its seed |

#### Observers

| Function | Description |
|---|---|
| [value](value.md) | the hash of the bytes so far |
| [digest](digest.md) | the hash as bytes, the most significant first |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](../mixin/hasher/README.md)).

| Function | Description |
|---|---|
| [update](../mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](../mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](../mixin/hasher/of.md) | the hash of bytes or a text in one call, with a seed or without (static) |
| [of_file, async_of_file](../mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{:016x}", hash::xxh3_64::of(""));
    println("{:016x}", hash::xxh3_64::of("hello"));
    println("{:016x}", hash::xxh3_64::of("hello", 42));

    // in pieces, the same value
    hash::xxh3_64 h(42);
    h.update("hel");
    h.update("lo");
    println("{:016x}", h.value());
}
```

Output:

```text
2d06800538d394c2
9555e8555c62dcfd
bafa072f07db7937
bafa072f07db7937
```

## See also

- [xxh3_128](../xxh3_128/README.md): the same with 128 bits
- [maphash](../maphash/README.md): the hash of a table in memory
- [siphash](../siphash/README.md): keys from an adversary
- [mixin::hasher](../mixin/hasher/README.md): the shape every hasher shares
- [sgcl::hash](../README.md)
