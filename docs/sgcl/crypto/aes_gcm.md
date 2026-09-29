# sgcl::crypto::aes_gcm

```cpp
#include "sgcl/crypto/gcm.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class aes_gcm;   // AES-128/192/256 in Galois/Counter Mode: nonce 12 bytes, tag 16
}
```

**The implementation has not been through an independent cryptographic audit.**

AES-GCM ([SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final)), the authenticated encryption of TLS, SSH, IPsec and most of what is encrypted today: AES in counter mode for secrecy, GHASH — a polynomial MAC over GF(2^128) — for integrity. A key of 128, 192 or 256 bits, a nonce of 96 bits, a tag of 128. Go's `cipher.NewGCM(aes.NewCipher(key))`. The interface is the module's [AEAD](aead.md): `seal`, `open`, `seal_to`, `open_to`.

## The nonce

**A nonce must never repeat under one key.** Two messages sealed with the same key and nonce give away the XOR of their plaintexts and, worse, GHASH's key, after which anyone can forge messages that open. The library cannot see a repeat; the program prevents it:

- **A counter**, [`nonce_counter`](nonce_counter.md): one per key, `next()` for every message. It cannot collide. This is what TLS does (the record number), and what SP 800-38D §8.2.1 calls the deterministic construction.
- **Random nonces** are safe for a limited number of messages only: 96 random bits collide with a probability that grows with the square of the count, and NIST allows 2^32 messages per key that way. Where a counter cannot be kept — many writers, no state — [`xchacha20_poly1305`](chacha20_poly1305.md) takes 192 random bits, which do not collide.

## Rules

- **The key** is copied into the object: the AES round keys and the powers H..H^8 of GHASH's key, in the object's own memory (about 500 bytes; no allocation). The object is **move-only**, a copy is `clone()`; its destructor and a move out of it overwrite that memory with zeros the compiler cannot remove. A key object belongs on the stack or in a `unique_ptr`: in a managed object it stays in memory until the collector's cycle finds the object dead, and only then is its destructor run.
- **Nonces of 12 bytes only.** GCM allows other lengths (hashed to a counter with GHASH), and Go has `NewGCMWithNonceSize` for old protocols; nothing new should use them, and the module does not have them. Tags shorter than 16 bytes are not here either.
- **Open is two passes**: GHASH over the ciphertext, the tag compared, then the decryption. See [aead.md](aead.md): nothing of a forgery is written.
- A key read from data (a file, a message) goes through `from_key`, which gives `errc::invalid_key` for a wrong length; the constructor's `std::invalid_argument` is for a length written into the program.

## Paths

On x86-64 with AES-NI and PCLMULQDQ the same shape runs on AESENC and PCLMULQDQ: eight blocks at a time, GHASH on the products of eight blocks summed and reduced once, seal in one pass. On arm64 with the crypto extension (every Apple core, and every arm64 processor that has it, asked at run time) AES runs on AESE/AESMC, eight blocks at a time, and GHASH on PMULL: the products of eight blocks by H^8..H summed and reduced once. Seal is one pass, the eight blocks encrypted and hashed from the registers. Elsewhere, and under `SGCL_CRYPTO_PORTABLE`, AES is **bitsliced** — four blocks as eight 64-bit planes, the S-box computed as the inverse in GF(2^8) and the affine map, no table anywhere — and GHASH multiplies with integer products of operands with holes in them; both in constant time, and two orders of magnitude slower (the S-box computed as x^254 is correct by construction, not the smallest circuit there is). The choice is made when the key is set up, from the processor alone, as in [hash](../hash/README.md); no build flag is needed, and the tests run the whole suite on both paths.

## SGCL and Go

| Go | sgcl::crypto | note |
|---|---|---|
| `aes.NewCipher(key)` + `cipher.NewGCM(block)` | `aes_gcm(key)` | one type; `from_key(key)` for a key from data |
| `aead.Seal(nil, nonce, plaintext, aad)` | `seal(nonce, plaintext, aad)` | |
| `aead.Seal(dst[:0], nonce, plaintext, aad)` | `seal_to(out, nonce, plaintext, aad)` | in place when `out` starts at the plaintext |
| `aead.Open(nil, nonce, sealed, aad)` | `open(nonce, sealed, aad)` | `expected`; the tag is checked before anything is written |
| `aead.NonceSize()`, `aead.Overhead()` | `nonce_size`, `overhead` | |
| `cipher.NewGCMWithNonceSize`, `NewGCMWithTagSize` | — | 12-byte nonces and 16-byte tags only |
| `cipher.NewGCMWithRandomNonce` (Go 1.24) | — | a random nonce is [`xchacha20_poly1305::seal_random`](chacha20_poly1305.md)'s job |

## Members

```cpp
static constexpr size_t nonce_size = 12;
static constexpr size_t tag_size = 16;
static constexpr size_t overhead = 16;
static constexpr uint64_t max_plaintext_size = (uint64_t(1) << 36) - 32;   // SP 800-38D: 2^39 - 256 bits under one nonce

explicit aes_gcm(const slice<const byte>& key);                        // 16, 24 or 32 bytes; else std::invalid_argument
static expected<aes_gcm, error> from_key(const slice<const byte>& key);  // a key from data: errc::invalid_key for a wrong length

aes_gcm(aes_gcm&&) noexcept;                  // move-only; the object moved from is zeroed
aes_gcm& operator=(aes_gcm&&) noexcept;
~aes_gcm();                                   // zeroes the key schedule and the hash key
aes_gcm clone() const;                        // a copy, made on purpose
size_t key_size() const noexcept;             // 16, 24 or 32; 0 after a move

// seal, open, seal_to, open_to: mixin::aead (aead.md)
```

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    // the key of McGrew and Viega's test case 4; a program's key comes from
    // a generator or from a key agreement, never from its source
    vector<byte> key = encoding::hex::decode("feffe9928665731c6d6a8f9467308308");
    crypto::aes_gcm gcm(key);
    crypto::nonce_counter nonces;  // one per key, for as long as the key lives

    string text = "attack at dawn";
    string header = "to: hq";  // sent in the clear, but authenticated

    auto nonce = nonces.next();
    auto sealed = gcm.seal(nonce, text, header);
    println("{} bytes: {}", sealed.size(), encoding::hex::encode(sealed));

    auto opened = gcm.open(nonce, sealed, header);
    println(string(opened));

    sealed[0] ^= byte(1);  // one bit of the ciphertext changed on the way
    auto forged = gcm.open(nonce, sealed, header);
    println(forged ? "opened" : forged.error().message());

    // in place, into a buffer the program owns: the plaintext, then room for the tag
    array<byte, 14 + 16> buffer;
    std::memcpy(buffer.data(), text.data(), 14);
    auto next = nonces.next();
    size_t n = gcm.seal_to(buffer, next, buffer.as_slice(0, 14));
    auto m = gcm.open_to(buffer, next, buffer.as_slice(0, n));
    println("{} {}", n, *m);
}
```

Output:

```text
30 bytes: 2bc4c083471b51be0ae8f9c5627604ff8b8bd87bb8e04f916adefa5e6781
attack at dawn
message authentication failed
30 14
```

## See also

[The module](README.md); [the AEAD interface](aead.md); [`nonce_counter`](nonce_counter.md); [`chacha20_poly1305`](chacha20_poly1305.md), the other AEAD; [`aes`](aes.md), the block cipher alone; [`aes_ctr`](aes_ctr.md), the counter mode without the tag.
