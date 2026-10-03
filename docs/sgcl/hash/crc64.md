[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::crc64

```cpp
#include "sgcl/hash/crc64.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class crc64;  // CRC-64/XZ
}
```

`sgcl::hash::crc64` is CRC-64/XZ of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/): the checksum
xz and 7z write, the one Go calls `crc64.ECMA`. The name is the catalogue's, not Go's, on purpose: the catalogue
has a CRC-64/ECMA-182 as well, over the same polynomial but neither reflected nor inverted, with another result for
the same bytes (`6C40DF5F0B497347` for the check string), and a `crc64_ecma` would send whoever checks the
catalogue to the wrong one. It is reflected, starts from all ones and inverts the result; [crc64_iso](crc64_iso.md)
is the other 64-bit CRC.

It has the shape of every hasher of the module ([mixin::hasher](mixin/hasher.md)): [update](crc64/update.md) as the
bytes come, [value](crc64/value.md) and [digest](crc64/digest.md) at any time, [of](mixin/hasher/of.md) for the
whole thing in one call, [copy_from](mixin/hasher/copy_from.md) and [of_file](mixin/hasher/of_file.md) for a stream
and a file. A CRC adds two of its own: [resume](crc64/resume.md), which goes on from a CRC saved earlier, and
[combine](crc64/combine.md), the CRC of two pieces from the CRCs of the pieces, with which a large buffer is hashed
on several threads.

## Rules

- **Eight bytes, the register.** The hasher holds that and nothing more: a plain value, trivially copyable, nothing
  for the collector, on a stack, in a field, in a managed object. A copy is a branch.
- **`value()` ends nothing**: `update` goes on after it. The CRC of nothing is 0.
- **`digest()` is big-endian**, as Go's `Sum` writes it. A block of xz stores the CRC the other way round, the
  least significant byte first: a format writes `value()` in its own order rather than taking `digest()`.
- **A value is not a seed.** `resume(v)` goes on from a CRC saved earlier. A one-argument constructor of the module
  ([xxh3_64](xxh3_64.md), [maphash](maphash.md)) is a seed, and a CRC has none: `crc64::of(data, v)` does not
  compile.
- **`combine` works on values.** It is static: a CRC does not count its own length, which would cost every `update`
  an addition for the sake of a call made once per piece, so the caller gives the length of the second piece.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `crc64.Checksum(p, crc64.MakeTable(crc64.ECMA))` | `crc64::of(p)` |
| `crc64.New(crc64.MakeTable(crc64.ECMA))` | `hash::crc64 h;` |
| `crc64.Update(crc, tab, p)` | `auto h = crc64::resume(crc); h.update(p);` |
| `h.Sum64()`, `h.Sum(nil)` | `h.value()`, `h.digest()`: the same big-endian bytes |
| — | `crc64::combine(a, b, n)`, zlib's `crc32_combine` for every CRC; Go has none |
| `crc64.MakeTable(poly)` with any polynomial | —: the four named CRCs only (`crc32`, `crc32c`, `crc64`, `crc64_iso`); the engine takes any reflected polynomial, and a type for another comes when one is asked for |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `8` | the size of [digest](crc64/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | a CRC takes the bytes one at a time, in any number, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](crc64/crc64.md) | a hasher of no bytes yet |
| [resume](crc64/resume.md) | a hasher going on from a CRC saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](crc64/update.md) | hashes bytes |
| [reset](crc64/reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](crc64/value.md) | the CRC of the bytes so far |
| [digest](crc64/digest.md) | the CRC as bytes, the most significant first |

#### Combining

| Function | Description |
|---|---|
| [combine](crc64/combine.md) | the CRC of two pieces from their CRCs (static) |

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
    println("{:016x}", hash::crc64::of("123456789"));

    // in pieces, as the bytes come
    hash::crc64 h;
    h.update("first line\n");
    uint64_t saved = h.value();
    h.update("second line\n");
    println("{:016x}", h.value());

    // going on from a saved CRC, and joining the CRCs of two pieces
    auto more = hash::crc64::resume(saved);
    more.update("second line\n");
    uint64_t joined = hash::crc64::combine(saved, hash::crc64::of("second line\n"), 12);
    println("{} {}", more.value() == h.value(), joined == h.value());
}
```

Output:

```text
995dc9bbdf1939fa
64daa2b2ef31e912
true true
```

## See also

- [crc64_iso](crc64_iso.md): Go's other 64-bit CRC
- [crc32](crc32.md): the 32-bit CRC of zlib and gzip
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
