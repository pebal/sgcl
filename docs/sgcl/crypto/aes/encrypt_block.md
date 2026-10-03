[sgcl](../../README.md) › [crypto](../README.md) › [aes](../aes.md)

# sgcl::crypto::aes::encrypt_block

```cpp
array<byte, 16> encrypt_block(const array<byte, 16>& in) const;
```

Encrypts one block under the key: FIPS 197's cipher, 10, 12 or 14 rounds by the length of the key. Go's
`block.Encrypt(dst, src)`, with the block returned by value.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the block to encrypt |

## Return value

The encrypted block.

## Complexity

Constant, and in constant time: nothing depends on the key or the block but values.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

A block encrypted on its own is the primitive a mode is built from, not a way to encrypt data: the same block
gives the same ciphertext every time. [aes_gcm](../aes_gcm.md) encrypts data.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // FIPS 197, appendix B
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> input = encoding::hex::decode("3243f6a8885a308d313198a2e0370734");
    array<byte, 16> block;
    for (int i : range(16)) {
        block[i] = input[i];
    }
    crypto::aes cipher(key);
    println("{}", encoding::hex::encode(cipher.encrypt_block(block)));
}
```

Output:

```text
3925841d02dc09fbdc118597196a0b32
```

## See also

- [decrypt_block](decrypt_block.md): decrypts one block
- [aes_ctr](../aes_ctr.md): AES as a stream cipher
- [sgcl::crypto::aes](../aes.md)
