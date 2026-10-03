[sgcl](../../README.md) › [hash](../README.md)

# sgcl::hash::fnv32

```cpp
#include "sgcl/hash/fnv.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class fnv32;  // FNV-1, 32 bits
}
```

`sgcl::hash::fnv32` is FNV-1 of 32 bits, a hash of Fowler, Noll and Vo
([draft-eastlake-fnv](https://datatracker.ietf.org/doc/draft-eastlake-fnv/)): a start value, the offset basis, and
for every byte a multiplication by a prime and an XOR of the byte. FNV-1 multiplies first and FNV-1a XORs first.
Go's `fnv.New32()`.

FNV is taken today to agree with values somebody already computed, and then the variant and the width are not a
choice, so all six are here, as Go has them: this one and [fnv32a](../fnv32a/README.md), [fnv64](../fnv64/README.md),
[fnv64a](../fnv64a/README.md), [fnv128](../fnv128/README.md), [fnv128a](../fnv128a/README.md). Each has the shape of every hasher
([mixin::hasher](../mixin/hasher/README.md)), and [resume](resume.md), which goes on from a value saved earlier.
Something new that only needs a hash whose values do not change takes [xxh3_64](../xxh3_64/README.md).

## Rules

- **Four bytes, the value.** The hasher holds it and nothing more: a plain value, trivially copyable, nothing for
  the collector. A copy is a branch.
- **The state is the value.** `resume(v)` goes on from a value saved earlier, as [crc32](../crc32/README.md) and
  [adler32](../adler32/README.md) do (Go does it through `UnmarshalBinary`). There is no `combine`: the value after A says
  nothing that would let the hash of B be joined to it.
- **`value()` ends nothing**: `update` goes on after it. The value of nothing is the offset basis, `0x811c9dc5`.
- **`digest()` is big-endian**, as Go's `Sum` writes it.
- **No seed.** `fnv32::of(data, v)` does not compile; a value to go on from goes to `resume`.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `fnv.New32()` | `hash::fnv32 h;` |
| `h.Write(p)`, then `h.Sum32()` | `h.update(p)`, then `h.value()`; in one call `fnv32::of(p)` |
| `h.Sum(nil)` | `h.digest()`: the same big-endian bytes |
| `UnmarshalBinary` of a state saved earlier | `fnv32::resume(v)`, from the value, which is the whole state |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `4` | the size of [digest](digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | FNV takes the bytes one at a time, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](fnv32.md) | a hasher of no bytes yet |
| [resume](resume.md) | a hasher going on from a value saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes |
| [reset](reset.md) | puts the hasher back as it was made |

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
| [of](../mixin/hasher/of.md) | the hash of bytes or a text in one call (static) |
| [of_file, async_of_file](../mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{:08x}", hash::fnv32::of("foobar"));

    // in pieces, as the bytes come
    hash::fnv32 h;
    h.update("foo");
    auto saved = h.value();
    h.update("bar");
    println("{:08x}", h.value());

    // going on from a value saved earlier
    auto more = hash::fnv32::resume(saved);
    more.update("bar");
    println("{}", more.value() == h.value());
}
```

Output:

```text
31f0b262
31f0b262
true
```

## See also

- [xxh3_64](../xxh3_64/README.md): a fast hash with fixed values, for anything new
- [mixin::hasher](../mixin/hasher/README.md): the shape every hasher shares
- [sgcl::hash](../README.md)
