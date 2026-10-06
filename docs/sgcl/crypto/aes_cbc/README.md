[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::aes_cbc

```cpp
#include "sgcl/crypto/cbc.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class aes_cbc;
}
```

`sgcl::crypto::aes_cbc` is AES in cipher block chaining mode (SP 800-38A §6.2), Go's `cipher.NewCBCEncrypter` and
`cipher.NewCBCDecrypter`: each block encrypted after the ciphertext before it is XORed in, the first after the IV.
[encrypt](encrypt.md) and [decrypt](decrypt.md) take a whole message and its PKCS #7 padding (RFC 5652 §6.3: one to
sixteen bytes, each the count of them); [encrypt_blocks](encrypt_blocks.md) and [decrypt_blocks](decrypt_blocks.md)
take whole blocks and carry the chain from call to call, Go's `CryptBlocks`, for formats that pad on their own (7z,
PDF, TLS 1.0's records).

**Unauthenticated.** Whoever can change the ciphertext changes the next block's plaintext bit for bit, and a reply
that tells a bad padding from a good one lets an attacker decrypt the whole message (the padding oracle). A program
encrypting data takes [aes_gcm](../aes_gcm/README.md); this type is for the formats that name CBC, with a MAC over the
ciphertext checked before anything is decrypted (encrypt-then-MAC).

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The IV must be unpredictable** for each message: [random::bytes](../random/bytes.md)(16), sent with the
  ciphertext. [reset](reset.md) starts the next message under the same key.
- **The calls continue one chain**: the chain is the last ciphertext block, whichever way it went through, so two
  calls of `encrypt_blocks` over 32 and 64 bytes are one over 96.
- **The state lives in the object**: the key schedules and the chain, in the object's own memory, zeroed by the
  destructor and by a move out of it. Move-only, a copy is [clone](clone.md). On the stack or in a `unique_ptr`, not
  in a managed object.
- **A key from data is a value, a key in the program a contract**: [from_key](from_key.md) gives
  `errc::invalid_key` for a key of the wrong length; an IV of the wrong length is `invalid_argument`, thrown.
- **A padding is checked in constant time**: the whole last block, with no early exit; every wrong padding is the
  same `errc::authentication`. That narrows a padding oracle, it does not close it: the MAC is checked first.
- **Not synchronized**: every call but `key_size` changes the chain; one thread at a time.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `block_size` | `16` | the bytes of a block, `static constexpr size_t` |
| `iv_size` | `16` | the bytes of the IV, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](aes_cbc.md) | sets up the key and the IV, or takes another object's over |
| `(destructor)` | overwrites the key schedules and the chain with zeros |
| [operator=](operator_assign.md) | takes another object's state over |
| [from_key](from_key.md) | sets up a key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy at the same point of the chain, made on purpose |
| [key_size](key_size.md) | 16, 24 or 32; 0 after a move |
| [reset](reset.md) | the chain back to an IV: the next message |

#### A message with padding

| Function | Description |
|---|---|
| [encrypt](encrypt.md) | a message padded by PKCS #7 and encrypted |
| [decrypt](decrypt.md) | a ciphertext decrypted and its padding checked and taken off |

#### Whole blocks

| Function | Description |
|---|---|
| [encrypt_blocks](encrypt_blocks.md) | whole blocks encrypted, the chain carried on |
| [decrypt_blocks](decrypt_blocks.md) | whole blocks decrypted, the chain carried on |

## Complexity

Linear in the bytes. On arm64 decryption takes eight blocks through AESD/AESIMC at once, the blocks being
independent; encryption, whose every block waits for the one before, runs one chain with the round keys held in
registers for the call and the plaintext XORed into the first round key off the chain's path. On x86-64 the same
with AES-NI; elsewhere, and under `SGCL_CRYPTO_PORTABLE`, the bitsliced AES of [aes](../aes/README.md). Constant time
on every path.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key = encoding::hex::decode("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    vector<byte> iv = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    crypto::aes_cbc encryptor(key, iv);
    vector<byte> sealed = encryptor.encrypt("attack at dawn");
    println(encoding::hex::encode(sealed));

    crypto::aes_cbc decryptor(key, iv);
    vector<byte> opened = decryptor.decrypt(sealed);
    println("{}", string(opened));
}
```

Output:

```text
9815750861fd78a00a12e60534ee0106
attack at dawn
```

## See also

- [aes_gcm](../aes_gcm/README.md): the authenticated mode
- [aes_ctr](../aes_ctr/README.md): AES as a stream cipher
- [aes](../aes/README.md): the block cipher alone
- [The module](../README.md)
