[sgcl](../../README.md) › [hash](../README.md)

# sgcl::hash::crc32

```cpp
#include "sgcl/hash/crc32.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class crc32;  // CRC-32/ISO-HDLC
}
```

`sgcl::hash::crc32` is CRC-32/ISO-HDLC of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/): the
checksum zlib, gzip, zip, PNG and Ethernet carry, the one Go calls `crc32.IEEE`. It is reflected, starts from all
ones and inverts the result, as [crc32c](../crc32c/README.md), its sibling over Castagnoli's polynomial, does; the two are
two types rather than one with a polynomial, because a program that needs one of them needs that one by name.

It has the shape of every hasher of the module ([mixin::hasher](../mixin/hasher/README.md)): [update](update.md) as the
bytes come, [value](value.md) and [digest](digest.md) at any time, [of](../mixin/hasher/of.md) for the
whole thing in one call, [copy_from](../mixin/hasher/copy_from.md) and [of_file](../mixin/hasher/of_file.md) for a stream
and a file. A CRC adds two of its own: [resume](resume.md), which goes on from a CRC saved earlier, and
[combine](combine.md), the CRC of two pieces from the CRCs of the pieces, with which a large buffer is hashed
on several threads.

## Rules

- **Four bytes, the register.** The hasher holds that and nothing more: a plain value, trivially copyable, nothing
  for the collector, on a stack, in a field, in a managed object. A copy is a branch.
- **`value()` ends nothing**: `update` goes on after it. The CRC of nothing is 0.
- **`digest()` is big-endian**, as Go's `Sum` writes it. A gzip member and a zip entry store the CRC the other way
  round, the least significant byte first, and PNG this way: a format writes `value()` in its own order rather than
  taking `digest()`.
- **A value is not a seed.** `resume(v)` goes on from a CRC saved earlier. A one-argument constructor of the module
  ([xxh3_64](../xxh3_64/README.md), [maphash](../maphash/README.md)) is a seed, and a CRC has none: `crc32::of(data, v)` does not
  compile.
- **`combine` works on values.** It is static: a CRC does not count its own length, which would cost every `update`
  an addition for the sake of a call made once per piece, so the caller gives the length of the second piece.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.
- **The names of zlib.** `crc32` and `adler32` are also the names of zlib's functions: `hash::crc32` qualified does
  not collide with them, a bare `crc32` under `using namespace sgcl::hash;` in a file that includes `zlib.h` does.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `crc32.ChecksumIEEE(p)` | `crc32::of(p)` |
| `crc32.NewIEEE()` | `hash::crc32 h;` |
| `crc32.Update(crc, crc32.IEEETable, p)` | `auto h = crc32::resume(crc); h.update(p);` |
| `h.Sum32()`, `h.Sum(nil)` | `h.value()`, `h.digest()`: the same big-endian bytes |
| — | `crc32::combine(a, b, n)`, zlib's `crc32_combine` for every CRC; Go has none |
| `crc32.MakeTable(poly)` with any polynomial | —: the four named CRCs only (`crc32`, `crc32c`, `crc64`, `crc64_iso`); the engine takes any reflected polynomial, and a type for another comes when one is asked for |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `4` | the size of [digest](digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | a CRC takes the bytes one at a time, in any number, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](crc32.md) | a hasher of no bytes yet |
| [resume](resume.md) | a hasher going on from a CRC saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes |
| [reset](reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](value.md) | the CRC of the bytes so far |
| [digest](digest.md) | the CRC as bytes, the most significant first |

#### Combining

| Function | Description |
|---|---|
| [combine](combine.md) | the CRC of two pieces from their CRCs (static) |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](../mixin/hasher/README.md)).

| Function | Description |
|---|---|
| [update](../mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](../mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](../mixin/hasher/of.md) | the CRC of bytes or a text in one call (static) |
| [of_file, async_of_file](../mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the catalogue's check: the CRC of the nine digits
    println("{:08x}", hash::crc32::of("123456789"));

    // in pieces, as the bytes come
    hash::crc32 h;
    h.update("first line\n");
    uint32_t saved = h.value();
    h.update("second line\n");
    println("{:08x}", h.value());

    // going on from a saved CRC, and joining the CRCs of two pieces
    auto more = hash::crc32::resume(saved);
    more.update("second line\n");
    uint32_t joined = hash::crc32::combine(saved, hash::crc32::of("second line\n"), 12);
    println("{} {}", more.value() == h.value(), joined == h.value());
}
```

Output:

```text
cbf43926
5d455a2c
true true
```

## See also

- [crc32c](../crc32c/README.md): the other 32-bit CRC, Castagnoli's
- [crc64](../crc64/README.md): the CRC of xz and 7z
- [adler32](../adler32/README.md): zlib's other checksum
- [mixin::hasher](../mixin/hasher/README.md): the shape every hasher shares
- [sgcl::hash](../README.md)
