[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::aes_ctr

```cpp
#include "sgcl/crypto/ctr.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class aes_ctr;
}
```

`sgcl::crypto::aes_ctr` is AES in counter mode ([SP 800-38A](https://csrc.nist.gov/pubs/sp/800/38/a/final) §6.5),
the stream cipher alone: the encryptions of a counter block, the block incremented as one 128-bit big-endian
number and wrapping at 2^128 (as Go's `cipher.NewCTR` and OpenSSL's `EVP_aes_*_ctr` count), XORed into the data.
The same call encrypts and decrypts. What Go's `cipher.NewCTR(aes.NewCipher(key), iv)` makes, with its
`XORKeyStream` as [xor_key_stream](xor_key_stream.md), and [seek](seek.md), which Go's CTR does
not have.

**Unauthenticated.** Whoever can change the ciphertext changes the plaintext bit for bit, and nothing notices. A
program encrypting data takes [aes_gcm](../aes_gcm/README.md). This type is for a protocol that authenticates by other means
(a MAC over the ciphertext: encrypt-then-MAC), for random access into a large encrypted file, and for tests.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A key and initial counter pair must never encrypt two messages**: the XOR of the two ciphertexts is the XOR of
  the two plaintexts.
- **The calls continue one another** at any length: 10 bytes and then 20 are the same as 30 at once. `seek` moves
  to any block, so that block *n* of a file decrypts without the blocks before it.
- **The state lives in the object.** The key schedule, the counter and the unused keystream of a block begun are in
  the object's own memory, with no allocation, and zeroed by the destructor and by a move out of it. Move-only, and
  a copy is [clone](clone.md), which goes on from the same place. On the stack or in a `unique_ptr`, not in
  a managed object, where it would stay in memory until the collector's cycle.
- **A key from data is a value, a key in the program a contract**: [from_key](from_key.md) gives
  `errc::invalid_key` for a key of the wrong length; an initial counter of the wrong length is still
  `invalid_argument`, thrown, since the counter is the program's to make.
- **Not synchronized**: `xor_key_stream` and `seek` change the object; one thread at a time.
- **Constant time on every path.** On arm64 eight blocks go through AESE/AESMC at once, the counters made with
  vector additions, and on x86-64 eight through AES-NI; elsewhere, and under `SGCL_CRYPTO_PORTABLE`, the bitsliced
  AES of [aes](../aes/README.md).

## Member objects

| Constant | Value | Description |
|---|---|---|
| `block_size` | `16` | the bytes of a block of the keystream, `static constexpr size_t` |
| `iv_size` | `16` | the bytes of the initial counter, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](aes_ctr.md) | sets up the key and the initial counter, or takes another object's over |
| `(destructor)` | overwrites the key schedule, the counter and the keystream with zeros |
| [operator=](operator_assign.md) | takes another object's state over |
| [from_key](from_key.md) | sets up a key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy that goes on from the same place, made on purpose |
| [key_size](key_size.md) | 16, 24 or 32; 0 after a move |

#### Keystream

| Function | Description |
|---|---|
| [xor_key_stream](xor_key_stream.md) | XORs the next bytes of the keystream into the data |
| [seek](seek.md) | moves to the start of a block |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.1, the first block; then block 2 on its own after a seek
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> text = encoding::hex::decode(
        "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52ef");
    crypto::aes_ctr ctr(key, iv);
    vector<byte> out(text.size());
    ctr.xor_key_stream(out, text);
    println("{}", encoding::hex::encode(out.as_slice(0, 16)));

    ctr.seek(2);
    array<byte, 16> third;
    ctr.xor_key_stream(third, out.as_slice(32, 16));
    println("{}", encoding::hex::encode(third));
}
```

Output:

```text
874d6191b620e3261bef6864990db6ce
30c81c46a35ce411e5fbc1191a0a52ef
```

## See also

- [aes_gcm](../aes_gcm/README.md): the same counter mode with a tag
- [aes](../aes/README.md): the block cipher alone
- [chacha20](../chacha20/README.md): the other stream cipher
- [The module](../README.md)
