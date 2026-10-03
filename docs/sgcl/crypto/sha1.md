[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::sha1

```cpp
#include "sgcl/crypto/sha1.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class sha1;
}
```

`sgcl::crypto::sha1` is SHA-1 of FIPS 180-4, Go's `crypto/sha1`: a digest of 20 bytes that has been broken for
collisions since 2017 (SHAttered: two PDFs with one digest) and is kept for what still names it — Git's object names,
HMAC-SHA-1 in TOTP (RFC 6238) and older protocols, PBKDF2-HMAC-SHA1 of RFC 6070. A new design takes
[sha256](sha256.md) or better; a signature or a certificate over SHA-1 is refused by the module's
[x509](x509.md).

It has the shape of every hasher of the [hash module](../hash/README.md), as [sha256](sha256.md) has it: `update`
takes bytes and text, `value()` is the digest as an `array<byte, 20>` and the hasher goes on, and a copy is Go's
`Clone`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shape of a hasher** of the [hash module](../hash/README.md), as [sha256](sha256.md) has it: a
  `crypto::sha1` goes wherever a `hash::req::hasher` is asked for.
- **A copy is a branch.** A hasher is the five chaining words, a block's buffer and the length, about 100 bytes,
  trivially copyable and nothing for the collector, so it lives anywhere a plain struct does.
- **HMAC-SHA-1 is not broken** by SHA-1's collisions: an HMAC needs the digest to be a pseudorandom function, which is
  another property. What cannot be trusted is a digest standing alone for its data where someone could have prepared
  two.
- **Hashing never fails and never waits**: `update`, `value`, `digest`, `reset` and `of` are `noexcept`; only the
  mixin's `copy_from` and `of_file` wait, for a stream or a file, and return its error.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `20` | the bytes of the digest, `static constexpr size_t` |
| `block_size` | `64` | the bytes of a block, what [hmac](hmac.md) pads its key to, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sha1/sha1.md) | a hasher of nothing yet |
| `(destructor)` | trivial: the state is not zeroed |

#### Hashing

| Function | Description |
|---|---|
| [update](sha1/update.md) | hashes bytes in |
| [value](sha1/value.md) | the digest of everything so far; the hasher goes on |
| [digest](sha1/digest.md) | the same bytes, under the name every hasher has |
| [reset](sha1/reset.md) | as new |

#### From mixin::hasher

The forms every hasher has ([hash::mixin::hasher](../hash/mixin/hasher.md)).

| Function | Description |
|---|---|
| `update` | text (a `string`, a text slice, a literal, a C string, a `std::string_view`), a digest, a `std::span` of bytes |
| `of` | the digest of data in one call (static) |
| `copy_from`, `async_copy_from` | a stream read to its end into the hasher |
| `of_file`, `async_of_file` | the digest of a whole file, or the file's error (static) |

## Complexity

Linear in the bytes hashed, one compression a block of 64. On arm64 the blocks go through the SHA-1 instructions of
ARMv8 (`SHA1C`, `SHA1P`, `SHA1M`, `SHA1H`, `SHA1SU0`, `SHA1SU1`), chosen as for [sha256](sha256.md#complexity)
(`FEAT_SHA1`, `HWCAP_SHA1`); plain C++ elsewhere and with `SGCL_CRYPTO_PORTABLE`.

## Example

The program prints what `git hash-object` prints for a file holding `hello`.

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

#include <array>

using namespace sgcl;

int main() {
    // Git's name of a blob: SHA-1 of "blob <size>", a zero byte and the contents
    string contents = "hello\n";
    crypto::sha1 h;
    h.update(txt::format("blob {}", contents.size()));
    h.update(std::array{byte(0)});
    h.update(contents);
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
ce013625030ba8dba906f756967f9e9ca394464a
```

## See also

- [sha256](sha256.md), [sha512](sha512.md), [sha3_256](sha3_256.md): the digests a new design takes
- [hmac](hmac.md), [pbkdf2](pbkdf2.md): where SHA-1 is still named
- [hash::mixin::hasher](../hash/mixin/hasher.md): the shape every hasher shares
- [The module](README.md)
