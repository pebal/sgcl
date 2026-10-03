[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::crc32c

```cpp
#include "sgcl/hash/crc32.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class crc32c;  // CRC-32/ISCSI
}
```

`sgcl::hash::crc32c` is CRC-32/ISCSI of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/),
Castagnoli's polynomial: the checksum of iSCSI, ext4, Btrfs, SCTP, LevelDB, RocksDB and gRPC, the one Go calls
`crc32.Castagnoli`. It is reflected, starts from all ones and inverts the result, as [crc32](crc32.md) does; the
two are two types rather than one with a polynomial, because a program that needs one of them needs that one by
name.

It has the shape of every hasher of the module ([mixin::hasher](mixin/hasher.md)): [update](crc32c/update.md) as
the bytes come, [value](crc32c/value.md) and [digest](crc32c/digest.md) at any time, [of](mixin/hasher/of.md) for
the whole thing in one call, [copy_from](mixin/hasher/copy_from.md) and [of_file](mixin/hasher/of_file.md) for a
stream and a file. A CRC adds two of its own: [resume](crc32c/resume.md), which goes on from a CRC saved earlier,
and [combine](crc32c/combine.md), the CRC of two pieces from the CRCs of the pieces, with which a large buffer is
hashed on several threads.

## Rules

- **Four bytes, the register.** The hasher holds that and nothing more: a plain value, trivially copyable, nothing
  for the collector, on a stack, in a field, in a managed object. A copy is a branch.
- **`value()` ends nothing**: `update` goes on after it. The CRC of nothing is 0.
- **`digest()` is big-endian**, as Go's `Sum` writes it. A format that stores the CRC the other way round, the
  least significant byte first, writes `value()` in its own order.
- **A value is not a seed.** `resume(v)` goes on from a CRC saved earlier. A one-argument constructor of the module
  ([xxh3_64](xxh3_64.md), [maphash](maphash.md)) is a seed, and a CRC has none: `crc32c::of(data, v)` does not
  compile.
- **`combine` works on values.** It is static: a CRC does not count its own length, which would cost every `update`
  an addition for the sake of a call made once per piece, so the caller gives the length of the second piece.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `crc32.Checksum(p, crc32.MakeTable(crc32.Castagnoli))` | `crc32c::of(p)` |
| `crc32.New(crc32.MakeTable(crc32.Castagnoli))` | `hash::crc32c h;` |
| `crc32.Update(crc, tab, p)` | `auto h = crc32c::resume(crc); h.update(p);` |
| `h.Sum32()`, `h.Sum(nil)` | `h.value()`, `h.digest()`: the same big-endian bytes |
| — | `crc32c::combine(a, b, n)`, zlib's `crc32_combine` for every CRC; Go has none |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `4` | the size of [digest](crc32c/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `1` | a CRC takes the bytes one at a time, in any number, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](crc32c/crc32c.md) | a hasher of no bytes yet |
| [resume](crc32c/resume.md) | a hasher going on from a CRC saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](crc32c/update.md) | hashes bytes |
| [reset](crc32c/reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](crc32c/value.md) | the CRC of the bytes so far |
| [digest](crc32c/digest.md) | the CRC as bytes, the most significant first |

#### Combining

| Function | Description |
|---|---|
| [combine](crc32c/combine.md) | the CRC of two pieces from their CRCs (static) |

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
    println("{:08x}", hash::crc32c::of("123456789"));

    // in pieces, as the bytes come
    hash::crc32c h;
    h.update("first line\n");
    uint32_t saved = h.value();
    h.update("second line\n");
    println("{:08x}", h.value());

    // going on from a saved CRC, and joining the CRCs of two pieces
    auto more = hash::crc32c::resume(saved);
    more.update("second line\n");
    uint32_t joined = hash::crc32c::combine(saved, hash::crc32c::of("second line\n"), 12);
    println("{} {}", more.value() == h.value(), joined == h.value());
}
```

Output:

```text
e3069283
e7d86cc6
true true
```

## See also

- [crc32](crc32.md): the CRC of zlib, gzip and zip
- [crc64](crc64.md): a 64-bit CRC
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
