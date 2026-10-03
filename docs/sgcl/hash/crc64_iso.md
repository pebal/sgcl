[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::crc64_iso

```cpp
#include "sgcl/hash/crc64.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class crc64_iso;  // CRC-64/GO-ISO
}
```

`sgcl::hash::crc64_iso` is CRC-64/GO-ISO of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/), the
one Go calls `crc64.ISO`: the polynomial of ISO 3309, reflected, starting from all ones and inverting the result.
It is here to agree with data Go wrote; it is a weak code for anything new, which takes [crc64](crc64.md).

It has the shape of every hasher of the module ([mixin::hasher](mixin/hasher.md)): [update](crc64_iso/update.md) as
the bytes come, [value](crc64_iso/value.md) and [digest](crc64_iso/digest.md) at any time, [of](mixin/hasher/of.md)
for the whole thing in one call, [copy_from](mixin/hasher/copy_from.md) and [of_file](mixin/hasher/of_file.md) for
a stream and a file. A CRC adds two of its own: [resume](crc64_iso/resume.md), which goes on from a CRC saved
earlier, and [combine](crc64_iso/combine.md), the CRC of two pieces from the CRCs of the pieces, with which a large
buffer is hashed on several threads.

## Rules

- **Eight bytes, the register.** The hasher holds that and nothing more: a plain value, trivially copyable, nothing
  for the collector, on a stack, in a field, in a managed object. A copy is a branch.
- **`value()` ends nothing**: `update` goes on after it. The CRC of nothing is 0.
- **`digest()` is big-endian**, as Go's `Sum` writes it. A format that stores the CRC the other way round, the
  least significant byte first, writes `value()` in its own order.
- **A value is not a seed.** `resume(v)` goes on from a CRC saved earlier. A one-argument constructor of the module
  ([xxh3_64](xxh3_64.md), [maphash](maphash.md)) is a seed, and a CRC has none: `crc64_iso::of(data, v)` does not
  compile.
- **`combine` works on values.** It is static: a CRC does not count its own length, which would cost every `update`
  an addition for the sake of a call made once per piece, so the caller gives the length of the second piece.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `crc64.Checksum(p, crc64.MakeTable(crc64.ISO))` | `crc64_iso::of(p)` |
| `crc64.New(crc64.MakeTable(crc64.ISO))` | `hash::crc64_iso h;` |
| `crc64.Update(crc, tab, p)` | `auto h = crc64_iso::resume(crc); h.update(p);` |
| `h.Sum64()`, `h.Sum(nil)` | `h.value()`, `h.digest()`: the same big-endian bytes |
| — | `crc64_iso::combine(a, b, n)`, zlib's `crc32_combine` for every CRC; Go has none |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](crc64_iso/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | a CRC takes the bytes one at a time, in any number, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](crc64_iso/crc64_iso.md) | a hasher of no bytes yet |
| [resume](crc64_iso/resume.md) | a hasher going on from a CRC saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](crc64_iso/update.md) | hashes bytes |
| [reset](crc64_iso/reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](crc64_iso/value.md) | the CRC of the bytes so far |
| [digest](crc64_iso/digest.md) | the CRC as bytes, the most significant first |

#### Combining

| Function | Description |
|---|---|
| [combine](crc64_iso/combine.md) | the CRC of two pieces from their CRCs (static) |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](mixin/hasher.md)).

| Function | Description |
|---|---|
| [update](mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](mixin/hasher/of.md) | the CRC of bytes or a text in one call (static) |
| [of_file, async_of_file](mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the catalogue's check: the CRC of the nine digits
    println("{:016x}", hash::crc64_iso::of("123456789"));

    // in pieces, as the bytes come
    hash::crc64_iso h;
    h.update("first line\n");
    uint64_t saved = h.value();
    h.update("second line\n");
    println("{:016x}", h.value());

    // going on from a saved CRC, and joining the CRCs of two pieces
    auto more = hash::crc64_iso::resume(saved);
    more.update("second line\n");
    uint64_t joined = hash::crc64_iso::combine(saved, hash::crc64_iso::of("second line\n"), 12);
    println("{} {}", more.value() == h.value(), joined == h.value());
}
```

Output:

```text
b90956c775a41001
bde4f66c12377da5
true true
```

## See also

- [crc64](crc64.md): the CRC of xz and 7z
- [crc32](crc32.md): the 32-bit CRC of zlib and gzip
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
