[sgcl](../../../README.md) › [crypto](../../README.md)

# sgcl::crypto::mixin::aead\<Derived\>

```cpp
#include "sgcl/crypto/mixin/aead.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mixin {
    template<class Derived>
    class aead;
}
```

`mixin::aead<Derived>` is the interface every authenticated cipher of the module shares,
[aes_gcm](../../aes_gcm/README.md), [chacha20_poly1305](../../chacha20_poly1305/README.md) and
[xchacha20_poly1305](../../xchacha20_poly1305/README.md): what Go's `cipher.AEAD` is. **Seal** encrypts a plaintext and
appends a tag that authenticates it together with additional data sent in the clear; **open** checks the tag and
gives the plaintext back, or an error when anything (the ciphertext, the tag, the nonce, the additional data or
the key) is not what was sealed. Each comes in two forms, into a new `vector<byte>` or into a buffer the caller
gives, with nothing allocated, and each with and without additional data.

The class supplies the two operations on raw bytes and the sizes (`nonce_size`, `tag_size`,
`max_plaintext_size`); the mixin checks the contract and gives the forms. Sealed data is the ciphertext followed
by the tag, `plaintext.size() + tag_size` bytes, as Go's `Seal` with a nil `dst` makes it. The nonce is not in
it: the program sends or stores the nonce beside it, or uses
[xchacha20_poly1305::seal_random](../../xchacha20_poly1305/seal_random.md), which writes it in front.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The contract**, a broken one being an exception, as everywhere in the library: the nonce is exactly
  `nonce_size` bytes (`invalid_argument`); the output of `seal_to` holds the plaintext and the tag, the output of
  `open_to` the sealed data less the tag (`length_error`); a plaintext is at most `max_plaintext_size` bytes
  (`length_error`); an object moved from holds no key (`logic_error`).
- **A failed open is a value.** `open` and `open_to` give [errc::authentication](../../errc.md) ("message
  authentication failed") for every kind of mismatch, for sealed data shorter than a tag and for sealed data longer
  than `max_plaintext_size + tag_size`, and say nothing more: which bytes were wrong is not an answer an attacker
  may have. Seal refuses a plaintext that long with `length_error`: a length the program chose is a broken
  contract, a length that came with the data is a forgery. Go panics on both.
- **In place.** `out` may be the input itself, the same first byte, and the work is done in place:
  `seal_to(buffer, nonce, buffer.as_slice(0, n))` with room for the tag after the text,
  `open_to(buffer, nonce, buffer)`. Any other overlap of `out` and the input is `invalid_argument`, as Go panics on it: a copy shifted by a
  few bytes would read what it has just written.
- **Nothing of a forgery is ever written.** Open computes the tag over the ciphertext and compares it in constant
  time, before the first byte is decrypted; when the tag does not match, the bytes `open_to` would have written are
  set to zero and nothing else of `out` is touched. A program that ignores the error reads zeros, neither the
  attacker's text nor the ciphertext; opened in place, the ciphertext is zeroed too. This costs a second pass over
  the data on open (the tag first, then the decryption), which one-pass implementations save by writing the
  plaintext first and wiping it on failure.
- **The plaintext of `open` is the user's data**, not key material: a managed `vector<byte>`, as `seal` gives the
  ciphertext, which the collector frees in its time and nobody zeroes. [secret_bytes](../../secret_bytes/README.md) is for
  keys, derived secrets, private keys' exports and passwords. A plaintext that must not stay in memory (a key
  unwrapped, a password) is opened with `open_to` into a buffer the program clears with
  [secure_zero](../../secure_zero.md) when done; a `secret_bytes` of the plaintext's length is one, given as it is,
  `open_to(s, ...)`.
- **`[[nodiscard]]`** on `open` and `open_to`: a dropped result of a verification is a hole, so the compiler warns
  about it, which the library does not do elsewhere.
- **One object, any number of threads.** The functions are `const` and keep nothing between calls: one key object
  seals and opens from any number of threads at once.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The AEAD that carries the mixin and names itself as the argument (`class aes_gcm : public mixin::aead<aes_gcm>`). It declares `nonce_size`, `tag_size` and `max_plaintext_size`, and the two operations on raw bytes the mixin calls. |

## Member functions

| Function | Description |
|---|---|
| [seal](seal.md) | encrypts and authenticates into a new vector: the ciphertext and the tag |
| [open](open.md) | checks the tag and decrypts into a new vector, or `errc::authentication` |
| [seal_to](seal_to.md) | encrypts and authenticates into the caller's buffer, in place too |
| [open_to](open_to.md) | checks the tag and decrypts into the caller's buffer, in place too |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any AEAD of the module, through the calls of the mixin
void round_trip(const auto& aead, const slice<const byte>& nonce) {
    string text = "attack at dawn";
    string header = "to: hq";  // sent in the clear, but authenticated
    auto sealed = aead.seal(nonce, text, header);
    auto opened = aead.open(nonce, sealed, header);
    string other = "to: all";
    auto forged = aead.open(nonce, sealed, other);
    println("{} bytes, {}, {}", sealed.size(), string(opened), forged.error().message());
}

int main() {
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("404142434445464748494a4b4c4d4e4f5051525354555657");

    round_trip(crypto::aes_gcm(key), nonce.as_slice(0, 12));
    round_trip(crypto::chacha20_poly1305(key), nonce.as_slice(0, 12));
    round_trip(crypto::xchacha20_poly1305(key), nonce);
}
```

Output:

```text
30 bytes, attack at dawn, message authentication failed
30 bytes, attack at dawn, message authentication failed
30 bytes, attack at dawn, message authentication failed
```

## See also

- [aes_gcm](../../aes_gcm/README.md), [chacha20_poly1305](../../chacha20_poly1305/README.md),
  [xchacha20_poly1305](../../xchacha20_poly1305/README.md): the classes that carry the mixin
- [nonce_counter](../../nonce_counter/README.md): nonces that never repeat under one key
- [error](../../error/README.md): the error of a failed open
- [The module](../../README.md)
