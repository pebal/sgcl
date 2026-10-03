[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::aes_gcm

```cpp
#include "sgcl/crypto/gcm.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class aes_gcm;
}
```

`sgcl::crypto::aes_gcm` is AES-GCM ([SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final)), the
authenticated encryption of TLS, SSH, IPsec and most of what is encrypted today: AES in counter mode for secrecy,
GHASH, a polynomial MAC over GF(2^128), for integrity. A key of 128, 192 or 256 bits, a nonce of 96 bits, a tag of
128. The interface is the module's AEAD, [mixin::aead](../mixin/aead/README.md): `seal`, `open`, `seal_to`, `open_to`.

Where Go builds the cipher in two steps, `cipher.NewGCM(aes.NewCipher(key))`, here it is one type, `aes_gcm(key)`,
and `from_key(key)` for a key that came with data. Go's `NonceSize()` and `Overhead()` are the constants
`nonce_size` and `overhead`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A nonce must never repeat under one key.** Two messages sealed with the same key and nonce give away the XOR
  of their plaintexts and, worse, GHASH's key, after which anyone can forge messages that open. The library cannot
  see a repeat; the program prevents it. A [nonce_counter](../nonce_counter/README.md), one per key with `next()` for every
  message, cannot collide: this is what TLS does with its record numbers, and what SP 800-38D §8.2.1 calls the
  deterministic construction. Random nonces are safe for a limited number of messages only: 96 random bits collide
  with a probability that grows with the square of the count, and NIST allows 2^32 messages per key that way. Where
  a counter cannot be kept (many writers, no state), [xchacha20_poly1305](../xchacha20_poly1305/README.md) takes 192 random
  bits, which do not collide; Go 1.24's `cipher.NewGCMWithRandomNonce` has no counterpart here for that reason.
- **Nonces of 12 bytes and tags of 16 only.** GCM allows other nonce lengths (hashed to a counter with GHASH), and
  Go has `NewGCMWithNonceSize` and `NewGCMWithTagSize` for old protocols; nothing new should use them, and the
  module does not have them.
- **The key lives in the object.** The AES round keys and the powers H to H^8 of GHASH's key are in the object's
  own memory, with no allocation. The object is move-only and a copy is [clone](clone.md); the destructor
  and a move out of it overwrite that memory with zeros the compiler cannot remove. A key object belongs on the
  stack or in a `unique_ptr`: in a managed object it stays in memory until the collector's cycle finds the object
  dead, and only then is its destructor run.
- **A key from data is a value, a key in the program a contract.** A key read from a file or a message goes
  through [from_key](from_key.md), which gives `errc::invalid_key` for a wrong length; the constructor's
  `invalid_argument` is for a length written into the program.
- **Open is two passes**: GHASH over the ciphertext, the tag compared, then the decryption. Nothing of a forgery is
  written ([mixin::aead](../mixin/aead/README.md)).
- **One object, any number of threads**: seal and open are `const` and keep nothing between calls.
- **Constant time on every path.** On arm64 with the crypto extension (every Apple core, and every arm64 processor
  that has it, asked at run time) AES runs on AESE/AESMC, eight blocks at a time, and GHASH on PMULL, the products
  of eight blocks by H^8 to H summed and reduced once; seal is one pass, the eight blocks encrypted and hashed from
  the registers. On x86-64 with AES-NI and PCLMULQDQ the same shape runs on AESENC and PCLMULQDQ. Elsewhere, and
  under `SGCL_CRYPTO_PORTABLE`, AES is bitsliced (four blocks as eight 64-bit planes, the S-box computed as the
  inverse in GF(2^8) and the affine map, no table anywhere) and GHASH multiplies with integer products of operands
  with holes in them; the S-box computed as x^254 is correct by construction, not the smallest circuit there is.
  The choice is made when the key is set up, from the processor alone; no build flag is needed, and the tests run
  the whole suite on both paths.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `nonce_size` | `12` | the bytes of a nonce, `static constexpr size_t` |
| `tag_size` | `16` | the bytes of a tag, `static constexpr size_t` |
| `overhead` | `16` | what seal adds to the plaintext, `static constexpr size_t` |
| `max_plaintext_size` | `(uint64_t(1) << 36) - 32` | the longest plaintext under one nonce, 2^39 - 256 bits (SP 800-38D §5.2.1.1), `static constexpr uint64_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](aes_gcm.md) | sets up the key, or takes another object's over |
| `(destructor)` | overwrites the round keys and the hash key with zeros |
| [operator=](operator_assign.md) | takes another object's key over |
| [from_key](from_key.md) | sets up a key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy of the key, made on purpose |
| [key_size](key_size.md) | 16, 24 or 32; 0 after a move |

#### From mixin::aead

The calls every AEAD of the module shares ([mixin::aead](../mixin/aead/README.md)).

| Function | Description |
|---|---|
| [seal](../mixin/aead/seal.md) | encrypts and authenticates into a new vector: the ciphertext and the tag |
| [open](../mixin/aead/open.md) | checks the tag and decrypts into a new vector, or `errc::authentication` |
| [seal_to](../mixin/aead/seal_to.md) | encrypts and authenticates into the caller's buffer, in place too |
| [open_to](../mixin/aead/open_to.md) | checks the tag and decrypts into the caller's buffer, in place too |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The key of the GCM specification's test case 4; a program's key comes from
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
    println("{}", string(opened));

    sealed[0] ^= byte(1);  // one bit of the ciphertext changed on the way
    auto forged = gcm.open(nonce, sealed, header);
    println("{}", forged ? "opened" : forged.error().message());
}
```

Output:

```text
30 bytes: 2bc4c083471b51be0ae8f9c5627604ff8b8bd87bb8e04f916adefa5e6781
attack at dawn
message authentication failed
```

## See also

- [mixin::aead](../mixin/aead/README.md): seal and open, the contract, in place
- [nonce_counter](../nonce_counter/README.md): the nonces of one key
- [chacha20_poly1305](../chacha20_poly1305/README.md): the other AEAD;
  [xchacha20_poly1305](../xchacha20_poly1305/README.md), the one with random nonces
- [aes](../aes/README.md): the block cipher alone; [aes_ctr](../aes_ctr/README.md): the counter mode without the tag
- [The module](../README.md)
