# sgcl::crypto::chacha20

```cpp
#include "sgcl/crypto/chacha20.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class chacha20;   // RFC 8439's ChaCha20 (nonce 12 bytes) or XChaCha20 (nonce 24)
}
```

**The implementation has not been through an independent cryptographic audit.**

ChaCha20 ([RFC 8439](https://www.rfc-editor.org/rfc/rfc8439) §2.4), the stream cipher alone: a keystream of 64-byte blocks made from the key, the nonce and a 32-bit block counter, XORed into the data; the same call encrypts and decrypts. With a 24-byte nonce it is XChaCha20: the key and the nonce's first 16 bytes through HChaCha20 to a new key, the last 8 bytes the nonce. Go's `chacha20.NewUnauthenticatedCipher`.

> **Unauthenticated.** Whoever can change the ciphertext changes the plaintext bit for bit, undetected. A program encrypting data takes [`chacha20_poly1305`](chacha20_poly1305.md). This type is for a protocol that authenticates by other means and for tests. **The key and nonce pair must never encrypt two messages.**

## Rules

- **The keystream starts at block 0.** RFC 8439's AEAD keeps block 0 for the Poly1305 key and encrypts from block 1, and its examples of the bare cipher start at 1: `seek(1)` first to reproduce them.
- **`xor_key_stream`** XORs `in` with the next `in.size()` bytes of keystream into `out`, which holds at least that many bytes (else `std::length_error`) and may be `in` itself (any other overlap is `std::invalid_argument`). Calls continue one another at any length.
- **The keystream is 2^32 blocks long** (256 GiB): a call that would need a block past the last is `std::length_error` and does nothing, as Go panics. A key and nonce pair is not for more than that.
- **`seek(counter)`** moves to the start of block `counter`, backwards too — Go's `SetCounter` refuses to go back, to stop a keystream from being reused by mistake; here a seek back is for decrypting from the middle, and reusing a keystream to encrypt is the program's mistake to avoid.
- The key, the counter and the unused keystream of a block begun are in the object and zeroed by the destructor and by a move out of it. As with every key object: on the stack or in a `unique_ptr`, not in a managed object, where it would stay in memory until the collector's cycle.
- **`from_key`** gives `errc::invalid_key` for a key of the wrong length, which may come with data; a nonce of the wrong length is still `std::invalid_argument`, thrown, since the nonce is the program's to make (the same split as [`aes_ctr::from_key`](aes_ctr.md)).
- On arm64 eight blocks go at once in NEON registers and a ninth in general registers; elsewhere a block at a time in C++. Constant-time on every machine: nothing but additions, XORs and rotations of values.

## SGCL and Go

| Go (`golang.org/x/crypto/chacha20`) | sgcl::crypto | note |
|---|---|---|
| `chacha20.NewUnauthenticatedCipher(key, nonce)` | `chacha20(key, nonce)` | a nonce of 12 or 24 bytes |
| `c.XORKeyStream(dst, src)` | `xor_key_stream(out, in)` | |
| `c.SetCounter(n)` | `seek(n)` | backwards too |
| `chacha20.HChaCha20(key, nonce)` | — | inside, for XChaCha20 |

## Members

```cpp
static constexpr size_t key_size = 32;
static constexpr size_t nonce_size = 12;
static constexpr size_t x_nonce_size = 24;
static constexpr size_t block_size = 64;

chacha20(const slice<const byte>& key, const slice<const byte>& nonce);      // key 32, nonce 12 or 24; else std::invalid_argument
static expected<chacha20, error> from_key(const slice<const byte>& key, const slice<const byte>& nonce);   // errc::invalid_key for the key

chacha20(chacha20&&) noexcept;         // move-only; the object moved from is zeroed
chacha20& operator=(chacha20&&) noexcept;
~chacha20();
chacha20 clone() const;                // the copy goes on from the same place

void xor_key_stream(const slice<byte>& out, const slice<const byte>& in);
void seek(uint32_t counter);           // the keystream from the start of block `counter` on
```

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.4.2: from block 1, in two calls
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000000000004a00000000");
    string text = "Ladies and Gentlemen of the class of '99";
    vector<byte> data(text.size());
    std::memcpy(data.data(), text.data(), text.size());
    crypto::chacha20 c(key, nonce);
    c.seek(1);
    c.xor_key_stream(data.as_slice(0, 10), data.as_slice(0, 10));  // in place
    c.xor_key_stream(data.as_slice(10), data.as_slice(10));
    println(encoding::hex::encode(data));
}
```

The output is the first 40 bytes of RFC 8439's ciphertext.

Output:

```text
6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab
```

## See also

[The module](README.md); [`chacha20_poly1305`](chacha20_poly1305.md), the cipher with its authenticator; [`aes_ctr`](aes_ctr.md).
