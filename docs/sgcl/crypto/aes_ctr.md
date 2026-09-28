# sgcl::crypto::aes_ctr

```cpp
#include "sgcl/crypto/ctr.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class aes_ctr;   // AES in counter mode: a keystream, the whole 16-byte block counting
}
```

**The implementation has not been through an independent cryptographic audit.**

AES in counter mode ([SP 800-38A](https://csrc.nist.gov/pubs/sp/800/38/a/final) §6.5), the stream cipher alone: AES of a counter block, the block incremented as one 128-bit big-endian number (wrapping at 2^128, as Go's `cipher.NewCTR` and OpenSSL's `EVP_aes_*_ctr` count), XORed into the data. The same call encrypts and decrypts.

> **Unauthenticated.** Whoever can change the ciphertext changes the plaintext bit for bit, and nothing notices. A program encrypting data takes [`aes_gcm`](aes_gcm.md). This type is for a protocol that authenticates by other means (a MAC over the ciphertext: encrypt-then-MAC), for random access into a large encrypted file (`seek`), and for tests. **The key and initial counter pair must never encrypt two messages**: the XOR of the two ciphertexts is the XOR of the two plaintexts.

## Members

```cpp
static constexpr size_t block_size = 16;
static constexpr size_t iv_size = 16;

aes_ctr(const slice<const byte>& key, const slice<const byte>& iv);        // key 16/24/32, iv 16; else std::invalid_argument
static expected<aes_ctr, error> from_key(const slice<const byte>& key, const slice<const byte>& iv);   // errc::invalid_key for the key

aes_ctr(aes_ctr&&) noexcept;         // move-only; the object moved from is zeroed
aes_ctr& operator=(aes_ctr&&) noexcept;
~aes_ctr();
aes_ctr clone() const;               // the copy goes on from the same place
size_t key_size() const noexcept;

void xor_key_stream(const slice<byte>& out, const slice<const byte>& in);
void seek(uint64_t block);           // the keystream from the initial counter + block on
```

## Rules

- **`xor_key_stream`** XORs `in` with the next `in.size()` bytes of keystream into `out`, which holds at least that many bytes (else `std::length_error`) and may be `in` itself; any other overlap is `std::invalid_argument`. Calls continue one another at any length: 10 bytes and then 20 are the same as 30 at once.
- **`seek(block)`** starts the keystream at the initial counter plus `block` (a 128-bit addition), so that block *n* of a file decrypts without the blocks before it. The part of a block left from the last call is dropped.
- The key schedule, the counter and the unused keystream of a block begun are in the object and zeroed by the destructor and by a move out of it.
- On arm64 eight blocks go through AESE/AESMC at once, the counters made with vector additions; elsewhere the bitsliced AES of [`aes`](aes.md).

## Example

```cpp
#include "sgcl/crypto/ctr.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.1, the first block; then block 2 on its own after a seek
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> text = encoding::hex::decode("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52ef");
    crypto::aes_ctr ctr(key, iv);
    vector<byte> out(text.size());
    ctr.xor_key_stream(out, text);
    println(encoding::hex::encode(out.as_slice(0, 16)));
    ctr.seek(2);
    array<byte, 16> third;
    ctr.xor_key_stream(third, out.as_slice(32, 16));
    println(encoding::hex::encode(third));
}
```

Output:

```text
874d6191b620e3261bef6864990db6ce
30c81c46a35ce411e5fbc1191a0a52ef
```

## SGCL and Go

| Go | sgcl::crypto | note |
|---|---|---|
| `cipher.NewCTR(aes.NewCipher(key), iv)` | `aes_ctr(key, iv)` | |
| `stream.XORKeyStream(dst, src)` | `xor_key_stream(out, in)` | |
| — | `seek(block)` | Go's CTR has no seek |

## See also

[The module](README.md); [`aes_gcm`](aes_gcm.md), the same counter mode with a tag; [`chacha20`](chacha20.md).
