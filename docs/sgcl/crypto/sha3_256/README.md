[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::sha3_256

```cpp
#include "sgcl/crypto/sha3.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class sha3_256;
    class sha3_224;
    class sha3_384;
    class sha3_512;
}
```

`sgcl::crypto::sha3_256` is SHA3-256 of FIPS 202, Go's `crypto/sha3`: the digest of the other family, the
Keccak-f[1600] sponge, where SHA-2 is a chain of compressions. `sha3_224`, `sha3_384` and `sha3_512` are the same
sponge with another rate (how many of the state's 200 bytes a block fills) and another length of output: their
`digest_size` is 28, 48 and 64, their `block_size` 144, 104 and 72, their `value()` an `array<byte, N>` of the
digest's size; everything else is as for `sha3_256`. The two extendable-output functions of the same standard are
[shake256](../shake256/README.md) and `shake128`.

A digest of SHA-3 is not one of Keccak: Ethereum's Keccak-256 pads otherwise and gives other values. Written from
FIPS 202 — the round constants and the rotation offsets are computed from the standard's own definitions — and tested
against its examples and against OpenSSL.

The four have the shape of every hasher of the [hash module](../../hash/README.md), as [sha256](../sha256/README.md) has it:
`update` takes bytes and text, `value()` is the digest and the hasher goes on, and a copy is Go's `Clone`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shape of a hasher** of the [hash module](../../hash/README.md), as [sha256](../sha256/README.md) has it: `update`,
  `value()`, `digest()`, `reset()`, `of`, `copy_from`. Each goes wherever a `hash::req::hasher` is asked for.
- **`block_size` is the rate**: 144, 136, 104 and 72 bytes for SHA3-224, -256, -384 and -512, what
  [hmac](../hmac/README.md) pads its key to.
- **Not open to length extension**, unlike SHA-2, so a keyed digest `sha3_256::of(key + message)` is a sound MAC;
  [hmac\<sha3_256\>](../hmac/README.md) is there for protocols that name it.
- **A copy is a branch.** A hasher is the 200 bytes of the state and a position, trivially copyable and nothing for
  the collector, so it lives anywhere a plain struct does. Its state is not zeroed when it dies.
- **Hashing never fails and never waits**: `update`, `value`, `digest`, `reset` and `of` are `noexcept`; only the
  mixin's `copy_from` and `of_file` wait, for a stream or a file, and return its error.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `digest_size` | `32` (`28`, `48`, `64` for `sha3_224`, `sha3_384`, `sha3_512`) | the bytes of the digest, `static constexpr size_t` |
| `block_size` | `136` (`144`, `104`, `72`) | the rate: the bytes of the state a block fills, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sha3_256.md) | a hasher of nothing yet |
| `(destructor)` | trivial: the state is not zeroed |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | hashes bytes in |
| [value](value.md) | the digest of everything so far; the hasher goes on |
| [digest](digest.md) | the same bytes, under the name every hasher has |
| [reset](reset.md) | as new |

#### From mixin::hasher

The forms every hasher has ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)).

| Function | Description |
|---|---|
| `update` | text (a `string`, a text slice, a literal, a C string, a `std::string_view`), a digest, a `std::span` of bytes |
| `of` | the digest of data in one call (static) |
| `copy_from`, `async_copy_from` | a stream read to its end into the hasher |
| `of_file`, `async_of_file` | the digest of a whole file, or the file's error (static) |

## Complexity

Linear in the bytes hashed, one permutation a block of the rate. On arm64 the permutation runs on the SHA-3
instructions of ARMv8.2: `EOR3` for θ's column sums, `RAX1` for its D, `XAR` for θ's XOR and ρ's rotation in one,
`BCAX` for χ, each of the 25 lanes in a register of its own and the state kept in registers across the blocks of an
update. The processor is asked for `FEAT_SHA3` (`HWCAP_SHA3`), apart from SHA-512's feature. Elsewhere, and with
`SGCL_CRYPTO_PORTABLE`, the permutation is plain C++; the tests run every vector on both paths and hold the two
against each other.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::sha3_256::of("abc")));
    println(encoding::hex::encode(crypto::sha3_224::of("abc")));
    println("{} {}", crypto::sha3_512::digest_size, crypto::sha3_512::block_size);
}
```

Output:

```text
3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf
64 72
```

## See also

- [shake256](../shake256/README.md): output of any length from the same sponge
- [sha256](../sha256/README.md), [sha512](../sha512/README.md): the SHA-2 digests
- [hmac](../hmac/README.md): a tag under a key
- [hash_id](../hash_id.md): a digest chosen at run time
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the shape every hasher shares
- [The module](../README.md)
