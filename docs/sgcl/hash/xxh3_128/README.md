[sgcl](../../README.md) › [hash](../README.md)

# sgcl::hash::xxh3_128

```cpp
#include "sgcl/hash/xxh3.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class xxh3_128;  // XXH3, 128 bits, with a seed
}
```

`sgcl::hash::xxh3_128` is XXH3 of 128 bits, the hash of xxHash 0.8, with a seed: what [xxh3_64](../xxh3_64/README.md) is, for
where 64 bits collide too often (content identifiers). Its values have been frozen since version 0.8.0 (2020) and
are the same on every machine, in every run and in every implementation: `xxhsum -H128`, Go's
`github.com/zeebo/xxh3`, Rust's `xxhash-rust`, Python's `xxhash`. It is the hash for what is written down or sent
away. Go's standard library has no XXH3; the Go column below is `zeebo/xxh3`.

It is not a defence against keys chosen by an adversary. A seed makes the values differ from the unseeded ones, not
unguessable: whoever sees a few values next to their inputs learns enough. A table keyed by what a client sends
takes [maphash](../maphash/README.md), whose values stay in the process, or, where the hashes may be seen,
[siphash](../siphash/README.md).

## Rules

- **The seed** is a number, 0 unless given: `xxh3_128(seed)`, `xxh3_128::of(data, seed)`. Two hashers with the same
  seed agree everywhere; a seed of 0 is XXH3 without one. There is no hash with a secret of the caller's own
  (xxHash's `_withSecret` functions): a seed is what the module offers.
- **`value()` ends nothing**: the hasher works the result out on a copy of its lanes and goes on.
- **The value is sixteen bytes**, the high half first, the canonical form `xxhsum -H128` prints; `value()` and
  `digest()` are the same.
- **`reset()` keeps the seed**: the hasher is as it was made.
- **552 bytes**: eight lanes, a buffer of four stripes (256 bytes) and the secret of the seed (192 bytes). A plain
  value, trivially copyable, nothing for the collector, so a copy is a branch. `of` makes no hasher.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `xxh3.Hash128(b)` | `xxh3_128::of(b)`: the 128-bit value as sixteen bytes, the high half first |
| `xxh3.Hash128Seed(b, seed)` | `xxh3_128::of(b, seed)` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `16` | the size of [digest](digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `64` | a stripe, the bytes the algorithm takes at a time on a long input, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xxh3_128.md) | a hasher of no bytes yet, with a seed or without |

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
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // sixteen bytes, as xxhsum -H128 prints them
    println("{}", encoding::hex::encode(hash::xxh3_128::of("hello")));
    println("{}", encoding::hex::encode(hash::xxh3_128::of("hello", 42)));

    // in pieces, the same value
    hash::xxh3_128 h(42);
    h.update("hel");
    h.update("lo");
    println("{}", h.value() == hash::xxh3_128::of("hello", 42));
}
```

Output:

```text
b5e9c1ad071b3e7fc779cfaa5e523818
6ce89a0bdba81f088c2f5b7e4cd59e16
true
```

## See also

- [xxh3_64](../xxh3_64/README.md): the same with 64 bits
- [maphash](../maphash/README.md): the hash of a table in memory
- [siphash](../siphash/README.md): keys from an adversary
- [mixin::hasher](../mixin/hasher/README.md): the shape every hasher shares
- [sgcl::hash](../README.md)
