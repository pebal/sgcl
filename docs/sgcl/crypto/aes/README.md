[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::aes

```cpp
#include "sgcl/crypto/aes.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class aes;
}
```

`sgcl::crypto::aes` is AES ([FIPS 197](https://csrc.nist.gov/pubs/fips/197/final)), the block cipher alone: one
block of 16 bytes encrypted or decrypted under a key of 16, 24 or 32 bytes. What Go's `aes.NewCipher(key)` makes,
with its `Encrypt` and `Decrypt` as [encrypt_block](encrypt_block.md) and
[decrypt_block](decrypt_block.md), which take and return a block by value; Go's `aes.BlockSize` is
`block_size`.

**This is not a way to encrypt data.** A block encrypted on its own is ECB: equal blocks of the message give equal
blocks of ciphertext, and the picture shows through. Nothing here authenticates anything either. A program
encrypting data takes [aes_gcm](../aes_gcm/README.md) (or [chacha20_poly1305](../chacha20_poly1305/README.md)); this type is for a
mode the module does not have (a key wrap, a CMAC, a protocol's own construction) and for tests.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The key schedule lives in the object**: the round keys, and on the processor's instructions the decryption's
  round keys too, in the object's own memory, with no allocation. The destructor and a move out of it overwrite it with zeros. Move-only, and a
  copy is [clone](clone.md); a call on an object moved from is `logic_error`. On the stack or in a
  `unique_ptr`, not in a managed object, where it would stay in memory until the collector's cycle.
- **A key from data is a value, a key in the program a contract**: [from_key](from_key.md) gives
  `errc::invalid_key` for a wrong length, the constructor throws `invalid_argument`.
- **The blocks are `array<byte, 16>` by value**; a copy on the stack is not zeroed, as in Go.
- **Constant time on every path.** On arm64 with the crypto extension a round is AESE/AESMC (AESD/AESIMC to
  decrypt), the S-box inside the processor; on x86-64 with AES-NI the same on AESENC and AESDEC. Elsewhere, and
  under `SGCL_CRYPTO_PORTABLE`, the cipher is bitsliced: the S-box is computed (the inverse in GF(2^8) as x^254,
  then the affine map) over 64-bit planes, and no table is read at an address that depends on the key or the data;
  the T-table AES of older libraries leaks both through the cache. The key schedule computes its S-box the same
  way. The path is chosen when the key is set up, from the processor alone.
- **One object, any number of threads**: the blocks are `const` calls that keep nothing between them.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `block_size` | `16` | the bytes of a block, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](aes.md) | sets up the key schedule, or takes another object's over |
| `(destructor)` | overwrites the key schedule with zeros |
| [operator=](operator_assign.md) | takes another object's key over |
| [from_key](from_key.md) | sets up a key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy of the key schedule, made on purpose |
| [key_size](key_size.md) | 16, 24 or 32; 0 after a move |

#### Blocks

| Function | Description |
|---|---|
| [encrypt_block](encrypt_block.md) | encrypts one block |
| [decrypt_block](decrypt_block.md) | decrypts one block |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // FIPS 197, appendix C.1
    vector<byte> key = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    crypto::aes cipher(key);
    array<byte, 16> block;
    for (int i : range(16)) {
        block[i] = byte(i * 0x11);
    }
    auto encrypted = cipher.encrypt_block(block);
    println("{}", encoding::hex::encode(encrypted));
    println("{}", cipher.decrypt_block(encrypted) == block);
}
```

Output:

```text
69c4e0d86a7b0430d8cdb78070b4c55a
true
```

## See also

- [aes_gcm](../aes_gcm/README.md): AES with a mode and a tag, the way to encrypt data
- [aes_ctr](../aes_ctr/README.md): AES in counter mode, unauthenticated
- [The module](../README.md)
