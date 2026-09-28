# sgcl::crypto::chacha20_poly1305, sgcl::crypto::xchacha20_poly1305

```cpp
#include "sgcl/crypto/chacha20_poly1305.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class chacha20_poly1305;    // RFC 8439: key 32 bytes, nonce 12, tag 16
    class xchacha20_poly1305;   // draft-irtf-cfrg-xchacha: the same with a nonce of 24 bytes
}
```

**The implementation has not been through an independent cryptographic audit.**

ChaCha20-Poly1305 ([RFC 8439](https://www.rfc-editor.org/rfc/rfc8439)), the other AEAD of TLS 1.3 and the one of WireGuard: ChaCha20 for secrecy, Poly1305 for integrity, a key of 256 bits, a nonce of 96, a tag of 128. It needs no special instructions to be fast and constant-time — additions, XORs and rotations, and a multiplication modulo 2^130 − 5 — so on a machine without AES instructions it is the faster AEAD by far. Go's `chacha20poly1305.New` and `NewX`. The interface is the module's [AEAD](aead.md): `seal`, `open`, `seal_to`, `open_to`.

**XChaCha20-Poly1305** ([draft-irtf-cfrg-xchacha-03](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha-03)) takes a nonce of 192 bits: the key and the nonce's first 16 bytes go through HChaCha20 to a key for the one message, the nonce's last 8 bytes are its nonce. 24 random bytes do not collide in any number of messages a program will ever seal, so a random nonce per message is safe, and `seal_random` draws it.

## The nonce

A nonce of `chacha20_poly1305` must **never repeat under one key**: a repeat gives away the XOR of the two plaintexts and the one-time Poly1305 key of that nonce, and with it forgeries. Give it a [`nonce_counter`](nonce_counter.md), or use `xchacha20_poly1305` with random nonces — `seal_random` does it and writes the nonce in front of the ciphertext, `open_random` reads it from there.

## Members

```cpp
static constexpr size_t key_size = 32;
static constexpr size_t nonce_size = 12;            // 24 for xchacha20_poly1305
static constexpr size_t tag_size = 16;
static constexpr size_t overhead = 16;
static constexpr uint64_t max_plaintext_size = ((uint64_t(1) << 32) - 1) * 64;   // the 32-bit block counter; block 0 is the Poly1305 key

explicit chacha20_poly1305(const slice<const byte>& key);                       // 32 bytes; else std::invalid_argument
static expected<chacha20_poly1305, error> from_key(const slice<const byte>& key); // a key from data: errc::invalid_key

chacha20_poly1305(chacha20_poly1305&&) noexcept;    // move-only; the object moved from is zeroed
chacha20_poly1305& operator=(chacha20_poly1305&&) noexcept;
~chacha20_poly1305();                                // zeroes the key
chacha20_poly1305 clone() const;

// seal, open, seal_to, open_to: mixin::aead (aead.md)

// xchacha20_poly1305 has the same, and:
vector<byte> seal_random(const slice<const byte>& plaintext) const;
vector<byte> seal_random(const slice<const byte>& plaintext, const slice<const byte>& aad) const;
[[nodiscard]] expected<vector<byte>, error> open_random(const slice<const byte>& sealed) const;
[[nodiscard]] expected<vector<byte>, error> open_random(const slice<const byte>& sealed, const slice<const byte>& aad) const;
```

`seal_random` gives `nonce || ciphertext || tag`, `plaintext.size() + 24 + 16` bytes, the nonce drawn from the system's generator (`getentropy`; a failure of the generator ends the program, as Go's `crypto/rand` does). `open_random` takes that and gives the plaintext, or `errc::authentication` — for data shorter than a nonce and a tag too.

## Rules

- **The key** is 32 bytes in the object's own memory, which the destructor and a move out of it overwrite with zeros; the object is move-only, a copy is `clone()`. As with every key object: on the stack or in a `unique_ptr`, not in a managed object, where it would stay in memory until the collector's cycle.
- **The per-message state** — the Poly1305 key (block 0 of the keystream), the HChaCha20 subkey of XChaCha, the keystream kept for the first blocks — lives on the stack for the call and is zeroed before it returns.
- **Open is two passes**, the tag over the ciphertext first, then the decryption; nothing of a forgery is written ([aead.md](aead.md)).
- **Seal** encrypts in pieces of about 4 KB and runs Poly1305 over each piece while it is still in the first level of the cache.

## Paths

On arm64 ChaCha20 runs on NEON: four blocks at once "vertically" (register *i* holding word *i* of the four), two such groups side by side and a ninth block in general registers in the same loop, so that the integer units work while the vector units are full; the rotations by 16 and 8 are byte permutations, those by 12 and 7 a shift and a shift-insert. Block 0 (the Poly1305 key) comes from the same batch as the first blocks of the text. Poly1305 is in C++ on both paths: limbs of 44, 44 and 42 bits, products of 64 × 64 → 128 bits, four blocks a step with r⁴, r³, r² and r, and the final reduction below p selected with a mask; nothing branches on the key. Under `SGCL_CRYPTO_PORTABLE` (and on targets without NEON) ChaCha20 is a block at a time in C++; the tests run the whole suite on both paths.

## Example

```cpp
#include "sgcl/crypto/chacha20_poly1305.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.8.2: its key, nonce and additional data, the first words of its text
    vector<byte> key = encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("070000004041424344454647");
    vector<byte> aad = encoding::hex::decode("50515253c0c1c2c3c4c5c6c7");
    string text = "Ladies and Gentlemen";

    crypto::chacha20_poly1305 aead(key);
    auto sealed = aead.seal(nonce, text, aad);
    println(encoding::hex::encode(sealed));
    auto opened = aead.open(nonce, sealed, aad);
    println(string(opened));

    // XChaCha20-Poly1305 with a random nonce, written before the ciphertext
    crypto::xchacha20_poly1305 x(key);
    auto a = x.seal_random(text);
    auto b = x.seal_random(text);
    println("{} bytes, the same twice: {}", a.size(), a == b);
    auto back = x.open_random(a);
    println(back.has_value());
}
```

Output (the ciphertext's first 20 bytes are RFC 8439's):

```text
d31a8d34648e60db7b86afbc53ef7ec2a4aded51e139d222c27617a282d78e210b635057
Ladies and Gentlemen
60 bytes, the same twice: false
true
```

## SGCL and Go

| Go (`golang.org/x/crypto/chacha20poly1305`) | sgcl::crypto | note |
|---|---|---|
| `chacha20poly1305.New(key)` | `chacha20_poly1305(key)` | `from_key(key)` for a key from data |
| `chacha20poly1305.NewX(key)` | `xchacha20_poly1305(key)` | |
| `aead.Seal(nil, nonce, plaintext, aad)` | `seal(nonce, plaintext, aad)` | |
| `aead.Open(nil, nonce, sealed, aad)` | `open(nonce, sealed, aad)` | `expected`; the tag is checked before anything is written |
| a random nonce, `append(nonce, aead.Seal(...)...)` | `xchacha20_poly1305::seal_random(plaintext, aad)` | and `open_random` |
| `chacha20poly1305.KeySize`, `NonceSize`, `NonceSizeX`, `Overhead` | `key_size`, `nonce_size`, `overhead` | |

## See also

[The module](README.md); [the AEAD interface](aead.md); [`nonce_counter`](nonce_counter.md); [`aes_gcm`](aes_gcm.md); [`chacha20`](chacha20.md), the stream cipher alone.
