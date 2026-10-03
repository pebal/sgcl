[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::chacha20_poly1305

```cpp
#include "sgcl/crypto/chacha20_poly1305.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class chacha20_poly1305;
}
```

`sgcl::crypto::chacha20_poly1305` is ChaCha20-Poly1305 ([RFC 8439](https://www.rfc-editor.org/rfc/rfc8439)
§2.8), the other AEAD of TLS 1.3 and the one of WireGuard: ChaCha20 for secrecy, Poly1305 for integrity, a key of
256 bits, a nonce of 96, a tag of 128. It needs no special instructions to be constant-time: additions, XORs and
rotations, and a multiplication modulo 2^130 - 5. The interface is the module's AEAD,
[mixin::aead](mixin/aead.md): `seal`, `open`, `seal_to`, `open_to`.

What Go's `golang.org/x/crypto/chacha20poly1305.New(key)` makes, with `from_key(key)` for a key that came with
data; Go's `KeySize`, `NonceSize` and `Overhead` are the constants `key_size`, `nonce_size` and `overhead`. The
same with a nonce of 24 bytes, which may be drawn at random, is
[xchacha20_poly1305](xchacha20_poly1305.md).

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A nonce must never repeat under one key.** A repeat gives away the XOR of the two plaintexts and the one-time
  Poly1305 key of that nonce, and with it forgeries. Give the key a [nonce_counter](nonce_counter.md); 12 random
  bytes are safe for a limited number of messages only, as with [aes_gcm](aes_gcm.md). Where a counter cannot be
  kept, [xchacha20_poly1305](xchacha20_poly1305.md) takes random nonces safely: its `seal_random` draws one and
  writes it in front of the ciphertext.
- **The key lives in the object.** 32 bytes in the object's own memory, which the destructor and a move out of it
  overwrite with zeros; the object is move-only, and a copy is [clone](chacha20_poly1305/clone.md). As with every
  key object: on the stack or in a `unique_ptr`, not in a managed object, where it would stay in memory until the
  collector's cycle.
- **The state of a message** (the Poly1305 key, which is block 0 of the keystream, and the keystream kept for the
  first blocks) lives on the stack for the call and is zeroed before it returns.
- **A key from data is a value, a key in the program a contract**: [from_key](chacha20_poly1305/from_key.md) gives
  `errc::invalid_key` for a wrong length, the constructor throws `invalid_argument`.
- **Open is two passes**, the tag over the ciphertext first, then the decryption; nothing of a forgery is written
  ([mixin::aead](mixin/aead.md)). **Seal** encrypts in pieces of about 4 KB and runs Poly1305 over each piece
  while it is still in the first level of the cache.
- **One object, any number of threads**: seal and open are `const` and keep nothing between calls.
- **Constant time on every path**, nothing branching on the key. On arm64 ChaCha20 runs on NEON: four blocks at
  once "vertically" (register *i* holding word *i* of the four), two such groups side by side and a ninth block in
  general registers in the same loop, so that the integer units work while the vector units are full; the
  rotations by 16 and 8 are byte permutations, those by 12 and 7 a shift and a shift-insert. Block 0, the Poly1305
  key, comes from the same batch as the first blocks of the text. On x86-64 the same shape runs four blocks in SSE2
  registers, eight in AVX2 ones where the processor has them. Poly1305 is C++ on every path: limbs of 44, 44 and 42
  bits, products of 64 × 64 to 128 bits, four blocks a step with r^4, r^3, r^2 and r, and the final reduction
  below p selected with a mask. Under `SGCL_CRYPTO_PORTABLE` ChaCha20 is a block at a time in C++; the tests run
  the whole suite on both paths.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `key_size` | `32` | the bytes of a key, `static constexpr size_t` |
| `nonce_size` | `12` | the bytes of a nonce, `static constexpr size_t` |
| `tag_size` | `16` | the bytes of a tag, `static constexpr size_t` |
| `overhead` | `16` | what seal adds to the plaintext, `static constexpr size_t` |
| `max_plaintext_size` | `((uint64_t(1) << 32) - 1) * 64` | the longest plaintext under one nonce: the block counter is 32 bits and block 0 is the Poly1305 key, `static constexpr uint64_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](chacha20_poly1305/chacha20_poly1305.md) | takes the key, or another object's over |
| `(destructor)` | overwrites the key with zeros |
| [operator=](chacha20_poly1305/operator_assign.md) | takes another object's key over |
| [from_key](chacha20_poly1305/from_key.md) | takes a key that came with data, into an `expected` (static) |
| [clone](chacha20_poly1305/clone.md) | a copy of the key, made on purpose |

#### From mixin::aead

The calls every AEAD of the module shares ([mixin::aead](mixin/aead.md)).

| Function | Description |
|---|---|
| [seal](mixin/aead/seal.md) | encrypts and authenticates into a new vector: the ciphertext and the tag |
| [open](mixin/aead/open.md) | checks the tag and decrypts into a new vector, or `errc::authentication` |
| [seal_to](mixin/aead/seal_to.md) | encrypts and authenticates into the caller's buffer, in place too |
| [open_to](mixin/aead/open_to.md) | checks the tag and decrypts into the caller's buffer, in place too |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.8.2: its key, nonce and additional data, the first words of its text;
    // the first 20 bytes of the ciphertext are the RFC's
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("070000004041424344454647");
    vector<byte> aad = encoding::hex::decode("50515253c0c1c2c3c4c5c6c7");
    string text = "Ladies and Gentlemen";

    crypto::chacha20_poly1305 aead(key);
    auto sealed = aead.seal(nonce, text, aad);
    println("{}", encoding::hex::encode(sealed));
    auto opened = aead.open(nonce, sealed, aad);
    println("{}", string(opened));
}
```

Output:

```text
d31a8d34648e60db7b86afbc53ef7ec2a4aded51e139d222c27617a282d78e210b635057
Ladies and Gentlemen
```

## See also

- [mixin::aead](mixin/aead.md): seal and open, the contract, in place
- [xchacha20_poly1305](xchacha20_poly1305.md): the same with a nonce of 24 bytes, random
- [nonce_counter](nonce_counter.md): the nonces of one key
- [aes_gcm](aes_gcm.md): the other AEAD
- [chacha20](chacha20.md): the stream cipher alone
- [The module](README.md)
