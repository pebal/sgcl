[sgcl](../../README.md) › [hash](../README.md)

# sgcl::hash::xxh32

```cpp
#include "sgcl/hash/xxhash.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class xxh32;  // XXH32, with a seed
}
```

`sgcl::hash::xxh32` is XXH32 of the xxHash specification, with a seed: the checksum LZ4's frame (its header, block and content checksums)
carries, and what file formats and protocols written before XXH3 name. Its values are fixed by the specification and
are the same on every machine and in every implementation: `xxhsum -H0`, libxxhash's `XXH32`, Python's
`xxhash.xxh32`. Go's standard library has no XXH32. A new design that only needs a fast hash whose values
are fixed takes [xxh3_64](../xxh3_64/README.md), faster on short inputs and no slower on long ones;
[xxh64](../xxh64/README.md) is the other width of the same family.

Like XXH3, it is not a defence against keys chosen by an adversary: a table keyed by what a client sends takes
[maphash](../maphash/README.md) or [siphash](../siphash/README.md).

## Rules

- **The seed** is a `uint32_t`, 0 unless given: `xxh32(seed)`, `xxh32::of(data, seed)`. Two hashers with the same
  seed agree everywhere; a seed of 0 is XXH32 without one.
- **`value()` ends nothing**: the hasher joins its lanes on a copy and goes on.
- **`digest()` is big-endian**, `value()` the most significant byte first, xxhsum's canonical form.
- **`reset()` keeps the seed**: the hasher is as it was made.
- **48 bytes**: four lanes, the seed, the length and up to a stripe of 16 bytes not yet taken. A plain value,
  trivially copyable, nothing for the collector, so a copy is a branch. `of` makes no hasher.
- **No `resume` and no `combine`**: the state is four lanes, not the value, so a saved value cannot be gone on from,
  and two pieces have no algebra that joins them; a copy within a process is the branch.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `4` | the size of [digest](digest.md) in bytes; `static constexpr size_t` |
| `block_size` | `16` | a stripe, four lanes of four bytes, the bytes the algorithm takes at a time; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xxh32.md) | a hasher of no bytes yet, with a seed or without |

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
    println("{:08x}", hash::xxh32::of(""));
    println("{:08x}", hash::xxh32::of("hello"));
    println("{:08x}", hash::xxh32::of("hello", 42));

    // in pieces, the same value
    hash::xxh32 h(42);
    h.update("hel");
    h.update("lo");
    println("{:08x}", h.value());
}
```

Output:

```text
02cc5d05
fb0077f9
4d02c966
4d02c966
```

## See also

- [xxh64](../xxh64/README.md): the other width of the family
- [xxh3_64](../xxh3_64/README.md): the newer hash of xxHash
- [mixin::hasher](../mixin/hasher/README.md): the shape every hasher shares
- [sgcl::hash](../README.md)
