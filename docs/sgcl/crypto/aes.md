# sgcl::crypto::aes

```cpp
#include "sgcl/crypto/aes.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class aes;   // the AES block cipher: one block of 16 bytes, keys of 128, 192 and 256 bits
}
```

**The implementation has not been through an independent cryptographic audit.**

AES ([FIPS 197](https://csrc.nist.gov/pubs/fips/197/final)), the block cipher alone: one block of 16 bytes encrypted or decrypted under a key of 16, 24 or 32 bytes. Go's `aes.NewCipher` and its `Encrypt` and `Decrypt`.

> **This is not a way to encrypt data.** A block encrypted on its own is ECB: equal blocks of the message give equal blocks of ciphertext, and the picture shows through. Nothing here authenticates anything either. A program encrypting data takes [`aes_gcm`](aes_gcm.md) (or [`chacha20_poly1305`](chacha20_poly1305.md)); this type is for a mode the module does not have — a key wrap, a CMAC, a protocol's own construction — and for tests.

## Members

```cpp
static constexpr size_t block_size = 16;

explicit aes(const slice<const byte>& key);                         // 16, 24 or 32 bytes; else std::invalid_argument
static expected<aes, error> from_key(const slice<const byte>& key);   // a key from data: errc::invalid_key

aes(aes&&) noexcept;                 // move-only; the object moved from is zeroed and holds no key
aes& operator=(aes&&) noexcept;
~aes();                              // zeroes the key schedule
aes clone() const;
size_t key_size() const noexcept;    // 16, 24 or 32; 0 after a move

array<byte, 16> encrypt_block(const array<byte, 16>& in) const;
array<byte, 16> decrypt_block(const array<byte, 16>& in) const;
```

## Rules

- **The key schedule** — the round keys, and on arm64 the decryption's round keys too — is in the object's own memory (no allocation); the destructor and a move out of it overwrite it with zeros. Move-only, `clone()` for a copy; a call on an object moved from is `std::logic_error`.
- **Constant time on both paths.** On arm64 a round is AESE/AESMC (AESD/AESIMC to decrypt), the S-box inside the processor. Elsewhere, and under `SGCL_CRYPTO_PORTABLE`, the cipher is bitsliced: the S-box is computed (the inverse in GF(2^8) as x^254, then the affine map) over 64-bit planes, and no table is read at an address that depends on the key or the data — the T-table AES of older libraries leaks both through the cache. The key schedule computes its S-box the same way.
- The blocks are `array<byte, 16>` by value; a copy on the stack is not zeroed, as in Go.

## Example

```cpp
#include "sgcl/core/range.h"
#include "sgcl/crypto/aes.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    // FIPS 197 Appendix C.1
    vector<byte> key = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    crypto::aes cipher(key);
    array<byte, 16> block;
    for (auto i : range(16)) {
        block[i] = byte(i * 0x11);
    }
    auto encrypted = cipher.encrypt_block(block);
    println(encoding::hex::encode(encrypted));
    println(cipher.decrypt_block(encrypted) == block);
}
```

Output:

```text
69c4e0d86a7b0430d8cdb78070b4c55a
true
```

## SGCL and Go

| Go (`crypto/aes`) | sgcl::crypto | note |
|---|---|---|
| `aes.NewCipher(key)` | `aes(key)`, `aes::from_key(key)` | |
| `block.Encrypt(dst, src)` | `encrypt_block(in)` | a block by value |
| `block.Decrypt(dst, src)` | `decrypt_block(in)` | |
| `aes.BlockSize` | `aes::block_size` | |

## See also

[The module](README.md); [`aes_gcm`](aes_gcm.md); [`aes_ctr`](aes_ctr.md).
