[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::chacha20

```cpp
#include "sgcl/crypto/chacha20.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class chacha20;
}
```

`sgcl::crypto::chacha20` is ChaCha20 ([RFC 8439](https://www.rfc-editor.org/rfc/rfc8439) §2.4), the stream cipher
alone: a keystream of 64-byte blocks made from the key, the nonce and a 32-bit block counter, XORed into the data;
the same call encrypts and decrypts. With a nonce of 24 bytes it is XChaCha20: the key and the nonce's first 16
bytes go through HChaCha20 to a new key, and the last 8 bytes are the nonce. What Go's
`chacha20.NewUnauthenticatedCipher(key, nonce)` makes, with its `XORKeyStream` as
[xor_key_stream](xor_key_stream.md) and its `SetCounter` as [seek](seek.md), backwards too; Go's
`chacha20.HChaCha20` is inside, for XChaCha20, and has no function of its own here.

**Unauthenticated.** Whoever can change the ciphertext changes the plaintext bit for bit, undetected. A program
encrypting data takes [chacha20_poly1305](../chacha20_poly1305/README.md). This type is for a protocol that authenticates by
other means and for tests.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A key and nonce pair must never encrypt two messages**: the XOR of the two ciphertexts is the XOR of the two
  plaintexts.
- **The keystream starts at block 0.** RFC 8439's AEAD keeps block 0 for the Poly1305 key and encrypts from block
  1, and its examples of the bare cipher start at 1: `seek(1)` first to reproduce them.
- **The keystream is 2^32 blocks long** (256 GiB): a call that would need a block past the last is `length_error`
  and does nothing, as Go panics. A key and nonce pair is not for more than that.
- **The calls continue one another** at any length, and `seek` moves to any block.
- **The state lives in the object.** The key, the counter and the unused keystream of a block begun are in the
  object's own memory, and zeroed by the destructor and by a move out of it. Move-only, and a copy is
  [clone](clone.md), which goes on from the same place. As with every key object: on the stack or in a
  `unique_ptr`, not in a managed object, where it would stay in memory until the collector's cycle.
- **A key from data is a value, a key in the program a contract**: [from_key](from_key.md) gives
  `errc::invalid_key` for a key of the wrong length, which may come with data; a nonce of the wrong length is
  still `invalid_argument`, thrown, since the nonce is the program's to make (the same split as
  [aes_ctr::from_key](../aes_ctr/from_key.md)).
- **Not synchronized**: `xor_key_stream` and `seek` change the object; one thread at a time.
- **Constant time on every machine**: nothing but additions, XORs and rotations of values. On arm64 eight blocks
  go at once in NEON registers and a ninth in general registers; on x86-64 four blocks in SSE2 registers, eight in
  AVX2 ones where the processor has them; elsewhere, and under `SGCL_CRYPTO_PORTABLE`, a block at a time in C++.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `key_size` | `32` | the bytes of a key, `static constexpr size_t` |
| `nonce_size` | `12` | the bytes of RFC 8439's nonce, `static constexpr size_t` |
| `x_nonce_size` | `24` | the bytes of XChaCha20's nonce, `static constexpr size_t` |
| `block_size` | `64` | the bytes of a block of the keystream, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](chacha20.md) | sets up the key and the nonce, or takes another object's over |
| `(destructor)` | overwrites the key, the counter and the keystream with zeros |
| [operator=](operator_assign.md) | takes another object's state over |
| [from_key](from_key.md) | sets up a key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy that goes on from the same place, made on purpose |

#### Keystream

| Function | Description |
|---|---|
| [xor_key_stream](xor_key_stream.md) | XORs the next bytes of the keystream into the data |
| [seek](seek.md) | moves to the start of a block, backwards too |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.4.2, from block 1: the first 40 bytes of its ciphertext
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000000000004a00000000");
    string text = "Ladies and Gentlemen of the class of '99";

    crypto::chacha20 cipher(key, nonce);
    cipher.seek(1);
    vector<byte> data(text.size());
    cipher.xor_key_stream(data, text);
    println("{}", encoding::hex::encode(data));

    cipher.seek(1);  // back, to decrypt
    cipher.xor_key_stream(data, data);
    println("{}", string(data));
}
```

Output:

```text
6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab
Ladies and Gentlemen of the class of '99
```

## See also

- [chacha20_poly1305](../chacha20_poly1305/README.md): the cipher with its authenticator;
  [xchacha20_poly1305](../xchacha20_poly1305/README.md), with a nonce of 24 bytes
- [aes_ctr](../aes_ctr/README.md): the other stream cipher
- [The module](../README.md)
