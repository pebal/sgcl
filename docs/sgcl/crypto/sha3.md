# sgcl::crypto::sha3_224 … sha3_512, sgcl::crypto::shake128, sgcl::crypto::shake256

```cpp
#include "sgcl/crypto/sha3.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class sha3_224;   // SHA3-224 (FIPS 202)    value(): array<byte, 28>
    class sha3_256;   // SHA3-256               value(): array<byte, 32>
    class sha3_384;   // SHA3-384               value(): array<byte, 48>
    class sha3_512;   // SHA3-512               value(): array<byte, 64>
    class shake128;   // SHAKE128, an extendable-output function: read(n) as many bytes as asked
    class shake256;   // SHAKE256
}
```

SHA-3, Go's `crypto/sha3`: four digests and two extendable-output functions (XOFs), all six the Keccak-f[1600] sponge of FIPS 202, differing in the rate (how many of the state's 200 bytes a block fills) and in the domain bits of the padding. A digest of SHA-3 is not one of Keccak: Ethereum's Keccak-256 pads otherwise and gives other values. SHAKE is what ML-KEM and ML-DSA are built on, and a general way to draw any number of bytes from a seed. Written from FIPS 202 — the round constants and the rotation offsets are computed from the standard's own definitions — and tested against its examples and against OpenSSL.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The digests are hashers** of the [hash module](../hash/README.md), as [`sha256`](sha256.md) is: `update`, `value()`, `digest()`, `reset()`, `of`, `copy_from`; a copy is a branch. `block_size` is the rate: 144, 136, 104 and 72 bytes. Unlike SHA-2, SHA-3 is not open to length extension, so a keyed digest `sha3_256::of(key + message)` is a sound MAC; [`hmac<sha3_256>`](hmac.md) is there for protocols that name it.
- **A SHAKE is not a hasher**: its output has no fixed length. `read(n)` gives the next `n` bytes and `read_to(out)` fills a buffer; the first read closes the input, and every read goes on where the last one stopped, so two reads of 16 bytes give what one read of 32 gives. `update` after a read is a broken contract: `std::invalid_argument`. `reset()` opens the input again. `of(data, n)` is the one-shot form.
- **Security of a SHAKE** is the smaller of its strength (128 or 256 bits) and half the output's bits for collisions: read at least 32 bytes of SHAKE128, 64 of SHAKE256, where collisions matter.
- **Size**: each holds the 200 bytes of the state and a position, a plain value that a copy branches (a SHAKE's copy reads on alike).

## Members

```cpp
// sha3_224, sha3_256, sha3_384, sha3_512
static constexpr size_t digest_size = 32;         // 28, 32, 48, 64
static constexpr size_t block_size = 136;         // 144, 136, 104, 72: the rate
sha3_256() noexcept;
void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
array<byte, 32> value() const noexcept;
array<byte, 32> digest() const noexcept;
void reset() noexcept;
static array<byte, 32> of(/* bytes or text */) noexcept;
expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
static expected<array<byte, 32>, io::error> of_file(const string& path);  static async::task<expected<array<byte, 32>, io::error>> async_of_file(const string& path);   // of() of the whole file, through copy_from

// shake128, shake256
static constexpr size_t block_size = 168;         // 136 for shake256
shake128() noexcept;
void update(const slice<const byte>& data);        // bytes or text; after a read: std::invalid_argument
vector<byte> read(size_t n);                      // the next n bytes
void read_to(const slice<byte>& out) noexcept;    // the next out.size() bytes, no allocation
void reset() noexcept;
static vector<byte> of(const slice<const byte>& data, size_t n);
```

## Paths

On arm64 the permutation runs on the SHA-3 instructions of ARMv8.2: `EOR3` for θ's column sums, `RAX1` for its D, `XAR` for θ's XOR and ρ's rotation in one, `BCAX` for χ, each of the 25 lanes in a register of its own and the state kept in registers across the blocks of an update. The processor is asked for `FEAT_SHA3` (`HWCAP_SHA3`), apart from SHA-512's feature. Elsewhere, and with `SGCL_CRYPTO_PORTABLE`, the permutation is plain C++; the tests run every vector on both paths and hold the two against each other.

## Example

```cpp
#include "sgcl/crypto/sha3.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::sha3_256::of("abc")));

    // SHAKE: as many bytes as asked, and more after them
    crypto::shake128 x;
    x.update("seed");
    auto first = x.read(16);
    auto next = x.read(16);
    println(encoding::hex::encode(first));
    println(encoding::hex::encode(next));
    println(encoding::hex::encode(crypto::shake128::of("seed", 32)));
}
```

Output:

```text
3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
25629347589242761d31f826ba4b757b
4f4f95668c83dfb6401762bb2d01a262
25629347589242761d31f826ba4b757b4f4f95668c83dfb6401762bb2d01a262
```

## See also

[`sha256`](sha256.md), [`sha512`](sha512.md); [`hmac`](hmac.md); [`hash_id`](hash_id.md).
