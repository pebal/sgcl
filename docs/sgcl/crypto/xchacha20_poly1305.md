[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::xchacha20_poly1305

```cpp
#include "sgcl/crypto/chacha20_poly1305.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class xchacha20_poly1305;
}
```

`sgcl::crypto::xchacha20_poly1305` is XChaCha20-Poly1305
([draft-irtf-cfrg-xchacha-03](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha-03)):
[chacha20_poly1305](chacha20_poly1305.md) with a nonce of 192 bits. The key and the nonce's first 16 bytes go
through HChaCha20 to a key for the one message, and the nonce's last 8 bytes are its nonce. 24 random bytes do not
collide in any number of messages a program will ever seal, so a random nonce per message is safe:
[seal_random](xchacha20_poly1305/seal_random.md) draws it and writes it in front of the ciphertext,
[open_random](xchacha20_poly1305/open_random.md) reads it from there. The rest of the interface is the module's
AEAD, [mixin::aead](mixin/aead.md): `seal`, `open`, `seal_to`, `open_to`, with a nonce of 24 bytes.

What Go's `golang.org/x/crypto/chacha20poly1305.NewX(key)` makes; what a Go program writes as a random nonce and
`append(nonce, aead.Seal(nil, nonce, plaintext, aad)...)` is `seal_random(plaintext, aad)` here. Go's
`NonceSizeX` is the constant `nonce_size`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The one AEAD of the module where a random nonce is safe** for any number of messages: the way to seal where a
  counter cannot be kept, many writers or no state. A nonce given to `seal` must still never repeat under one key.
- **The key lives in the object.** 32 bytes in the object's own memory, which the destructor and a move out of it
  overwrite with zeros; the object is move-only, and a copy is [clone](xchacha20_poly1305/clone.md). On the stack
  or in a `unique_ptr`, not in a managed object, where it would stay in memory until the collector's cycle.
- **The state of a message** (the HChaCha20 subkey, the Poly1305 key and the keystream kept for the first blocks)
  lives on the stack for the call and is zeroed before it returns.
- **A key from data is a value, a key in the program a contract**: [from_key](xchacha20_poly1305/from_key.md)
  gives `errc::invalid_key` for a wrong length, the constructor throws `invalid_argument`.
- **Open is two passes**, the tag first, then the decryption; nothing of a forgery is written
  ([mixin::aead](mixin/aead.md)). One object seals and opens from any number of threads at once.
- **Constant time**, on the paths of [chacha20_poly1305](chacha20_poly1305.md): HChaCha20 is ChaCha20's rounds
  without the final addition.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `key_size` | `32` | the bytes of a key, `static constexpr size_t` |
| `nonce_size` | `24` | the bytes of a nonce, `static constexpr size_t` |
| `tag_size` | `16` | the bytes of a tag, `static constexpr size_t` |
| `overhead` | `16` | what seal adds to the plaintext; `seal_random` adds `nonce_size` more, `static constexpr size_t` |
| `max_plaintext_size` | `((uint64_t(1) << 32) - 1) * 64` | the longest plaintext under one nonce: the block counter is 32 bits and block 0 is the Poly1305 key, `static constexpr uint64_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xchacha20_poly1305/xchacha20_poly1305.md) | takes the key, or another object's over |
| `(destructor)` | overwrites the key with zeros |
| [operator=](xchacha20_poly1305/operator_assign.md) | takes another object's key over |
| [from_key](xchacha20_poly1305/from_key.md) | takes a key that came with data, into an `expected` (static) |
| [clone](xchacha20_poly1305/clone.md) | a copy of the key, made on purpose |

#### Random nonces

| Function | Description |
|---|---|
| [seal_random](xchacha20_poly1305/seal_random.md) | seals under a random nonce, written in front: nonce, ciphertext, tag |
| [open_random](xchacha20_poly1305/open_random.md) | opens what `seal_random` made, the nonce read from the front |

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
    // The draft's test vector §A.3.1, its key, nonce and additional data, the first words
    // of its text; the first 20 bytes of the ciphertext are the draft's
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("404142434445464748494a4b4c4d4e4f5051525354555657");
    vector<byte> aad = encoding::hex::decode("50515253c0c1c2c3c4c5c6c7");
    string text = "Ladies and Gentlemen";

    crypto::xchacha20_poly1305 aead(key);
    auto sealed = aead.seal(nonce, text, aad);
    println("{}", encoding::hex::encode(sealed.as_slice(0, 20)));

    // A random nonce for each message, written before the ciphertext
    auto a = aead.seal_random(text);
    auto b = aead.seal_random(text);
    println("{} bytes, the same twice: {}", a.size(), a == b);
    println("{}", string(aead.open_random(a)));
}
```

Output:

```text
bd6d179d3e83d43b9576579493c0e939572a1700
60 bytes, the same twice: false
Ladies and Gentlemen
```

## See also

- [mixin::aead](mixin/aead.md): seal and open, the contract, in place
- [chacha20_poly1305](chacha20_poly1305.md): the same with a nonce of 12 bytes
- [chacha20](chacha20.md): XChaCha20 alone, with a nonce of 24 bytes
- [random](random.md): the system's generator the nonces come from
- [The module](README.md)
