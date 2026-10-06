[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::decrypt_blocks

```cpp
void decrypt_blocks(const slice<byte>& out, const slice<const byte>& in);
```

Decrypts the whole blocks of `in` into `out`, the chain carried on from the last call and on to the next, nothing
checked or taken off: Go's `CryptBlocks` of a decrypter. `out` holds at least `in.size()` bytes and may be `in`
itself.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the plaintext goes, at least `in.size()` bytes |
| `in` | whole blocks of ciphertext |

## Return value

None.

## Complexity

Linear in `in.size()`; eight blocks at a time on the AES instructions, the blocks being independent.

## Exceptions

- `invalid_argument` when `in.size()` is not a multiple of 16, or `out` overlaps `in` other than exactly.
- `length_error` when `out` is shorter than `in`.
- `logic_error` when the object was moved from.

Nothing is written when one is thrown.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.2.2: the first block back
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    vector<byte> block = encoding::hex::decode("7649abac8119b246cee98e9b12e9197d");
    crypto::aes_cbc cbc(key, iv);
    cbc.decrypt_blocks(block, block);
    println(encoding::hex::encode(block));
}
```

Output:

```text
6bc1bee22e409f96e93d7e117393172a
```

## See also

- [encrypt_blocks](encrypt_blocks.md): the other way
- [decrypt](decrypt.md): a message with its padding
- [sgcl::crypto::aes_cbc](README.md)
