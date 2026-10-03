[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::fnv64a

```cpp
#include "sgcl/hash/fnv.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class fnv64a;  // FNV-1a, 64 bits
}
```

`sgcl::hash::fnv64a` is FNV-1a of 64 bits, a hash of Fowler, Noll and Vo
([draft-eastlake-fnv](https://datatracker.ietf.org/doc/draft-eastlake-fnv/)): a start value, the offset basis, and
for every byte a multiplication by a prime and an XOR of the byte. FNV-1 multiplies first and FNV-1a XORs first.
Go's `fnv.New64a()`.

FNV is taken today to agree with values somebody already computed, and then the variant and the width are not a
choice, so all six are here, as Go has them: this one and [fnv32](fnv32.md), [fnv32a](fnv32a.md),
[fnv64](fnv64.md), [fnv128](fnv128.md), [fnv128a](fnv128a.md). Each has the shape of every hasher
([mixin::hasher](mixin/hasher.md)), and [resume](fnv64a/resume.md), which goes on from a value saved earlier.
Something new that only needs a hash whose values do not change takes [xxh3_64](xxh3_64.md).

## Rules

- **Eight bytes, the value.** The hasher holds it and nothing more: a plain value, trivially copyable, nothing for
  the collector. A copy is a branch.
- **The state is the value.** `resume(v)` goes on from a value saved earlier, as [crc32](crc32.md) and
  [adler32](adler32.md) do (Go does it through `UnmarshalBinary`). There is no `combine`: the value after A says
  nothing that would let the hash of B be joined to it.
- **`value()` ends nothing**: `update` goes on after it. The value of nothing is the offset basis,
  `0xcbf29ce484222325`.
- **`digest()` is big-endian**, as Go's `Sum` writes it.
- **No seed.** `fnv64a::of(data, v)` does not compile; a value to go on from goes to `resume`.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `fnv.New64a()` | `hash::fnv64a h;` |
| `h.Write(p)`, then `h.Sum64()` | `h.update(p)`, then `h.value()`; in one call `fnv64a::of(p)` |
| `h.Sum(nil)` | `h.digest()`: the same big-endian bytes |
| `UnmarshalBinary` of a state saved earlier | `fnv64a::resume(v)`, from the value, which is the whole state |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](fnv64a/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | FNV takes the bytes one at a time, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](fnv64a/fnv64a.md) | a hasher of no bytes yet |
| [resume](fnv64a/resume.md) | a hasher going on from a value saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](fnv64a/update.md) | hashes bytes |
| [reset](fnv64a/reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](fnv64a/value.md) | the hash of the bytes so far |
| [digest](fnv64a/digest.md) | the hash as bytes, the most significant first |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](mixin/hasher.md)).

| Function | Description |
|---|---|
| [update](mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](mixin/hasher/of.md) | the hash of bytes or a text in one call (static) |
| [of_file, async_of_file](mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{:016x}", hash::fnv64a::of("foobar"));

    // in pieces, as the bytes come
    hash::fnv64a h;
    h.update("foo");
    auto saved = h.value();
    h.update("bar");
    println("{:016x}", h.value());

    // going on from a value saved earlier
    auto more = hash::fnv64a::resume(saved);
    more.update("bar");
    println("{}", more.value() == h.value());
}
```

Output:

```text
85944171f73967e8
85944171f73967e8
true
```

## See also

- [xxh3_64](xxh3_64.md): a fast hash with fixed values, for anything new
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
