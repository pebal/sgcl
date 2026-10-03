[sgcl](../README.md) › [hash](README.md)

# sgcl::hash::adler32

```cpp
#include "sgcl/hash/adler32.h"   // or "sgcl/hash.h"

namespace sgcl::hash {
    class adler32;  // RFC 1950
}
```

`sgcl::hash::adler32` is Adler-32 (RFC 1950): two sums modulo 65521, the largest prime below 2^16. `a` is one plus
the bytes and `b` the sum of every `a` along the way; the checksum is `b` in the high half and `a` in the low. It
is what a zlib stream ends with, Go's `hash/adler32`, and it checks weakly on short inputs, where a [CRC](crc32.md)
is the better code.

It has the shape of every hasher of the module ([mixin::hasher](mixin/hasher.md)): [update](adler32/update.md) as
the bytes come, [value](adler32/value.md) and [digest](adler32/digest.md) at any time, [of](mixin/hasher/of.md) for
the whole thing in one call, [copy_from](mixin/hasher/copy_from.md) and [of_file](mixin/hasher/of_file.md) for a
stream and a file. A checksum adds two of its own: [resume](adler32/resume.md), which goes on from a checksum saved
earlier, and [combine](adler32/combine.md), the checksum of two pieces from the checksums of the pieces, with which
a large buffer is hashed on several threads.

## Rules

- **Four bytes, the two sums.** The hasher holds them and nothing more: a plain value, trivially copyable, nothing
  for the collector. A copy is a branch.
- **`value()` ends nothing**: `update` goes on after it. The checksum of nothing is 1.
- **`digest()` is big-endian**, what zlib writes at the end of a stream and Go's `Sum` gives.
- **A value is not a seed.** `resume(v)` goes on from a checksum saved earlier; `adler32::of(data, v)` does not
  compile. A value whose halves are 65521 or more is no checksum, and `resume` takes each half modulo 65521.
- **`combine` works on values**, static, as the CRCs' does: a few operations, whatever the length.
- **Nothing fails** but `copy_from` and `of_file`, which read a stream and a file and return its error.
- **The names of zlib.** `crc32` and `adler32` are also the names of zlib's functions: `hash::adler32` qualified
  does not collide with them, a bare `adler32` under `using namespace sgcl::hash;` in a file that includes `zlib.h`
  does.

### From code written for Go

| With Go | With sgcl::hash |
|---|---|
| `adler32.Checksum(p)` | `adler32::of(p)` |
| `adler32.New()` | `hash::adler32 h;` |
| `h.Sum32()`, `h.Sum(nil)` | `h.value()`, `h.digest()`: the same big-endian bytes |
| `UnmarshalBinary` of a state saved earlier | `adler32::resume(v)`, from the checksum, which is the whole state |
| — | `adler32::combine(a, b, n)`, zlib's `adler32_combine`; Go has none |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `4` | the size of [digest](adler32/digest.md) in bytes, Go's `Size()`; `static constexpr size_t` |
| `block_size` | `4` | the block Go's Adler-32 reports; the bytes go in in any number, Go's `BlockSize()`; `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](adler32/adler32.md) | a hasher of no bytes yet |
| [resume](adler32/resume.md) | a hasher going on from a checksum saved earlier (static) |

#### Modifiers

| Function | Description |
|---|---|
| [update](adler32/update.md) | hashes bytes |
| [reset](adler32/reset.md) | puts the hasher back as it was made |

#### Observers

| Function | Description |
|---|---|
| [value](adler32/value.md) | the checksum of the bytes so far |
| [digest](adler32/digest.md) | the checksum as bytes, the most significant first |

#### Combining

| Function | Description |
|---|---|
| [combine](adler32/combine.md) | the checksum of two pieces from their checksums (static) |

#### From mixin::hasher

The rest of the shape every hasher shares ([mixin::hasher](mixin/hasher.md)).

| Function | Description |
|---|---|
| [update](mixin/hasher/update.md) | hashes a text, a digest or a std::span of bytes |
| [copy_from, async_copy_from](mixin/hasher/copy_from.md) | hashes a stream to its end |
| [of](mixin/hasher/of.md) | the checksum of bytes or a text in one call (static) |
| [of_file, async_of_file](mixin/hasher/of_file.md) | the hash of a whole file (static) |

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{:08x}", hash::adler32::of("Wikipedia"));

    // in pieces, as the bytes come
    hash::adler32 h;
    h.update("Wiki");
    uint32_t saved = h.value();
    h.update("pedia");
    println("{:08x}", h.value());

    // going on from a saved checksum, and joining the checksums of two pieces
    auto more = hash::adler32::resume(saved);
    more.update("pedia");
    uint32_t joined = hash::adler32::combine(saved, hash::adler32::of("pedia"), 5);
    println("{} {}", more.value() == h.value(), joined == h.value());
}
```

Output:

```text
11e60398
11e60398
true true
```

## See also

- [crc32](crc32.md): gzip's checksum where Adler-32 is zlib's
- [mixin::hasher](mixin/hasher.md): the shape every hasher shares
- [sgcl::hash](README.md)
