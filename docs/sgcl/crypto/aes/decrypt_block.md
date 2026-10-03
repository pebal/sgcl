[sgcl](../../README.md) › [crypto](../README.md) › [aes](../aes.md)

# sgcl::crypto::aes::decrypt_block

```cpp
array<byte, 16> decrypt_block(const array<byte, 16>& in) const;
```

Decrypts one block under the key: FIPS 197's inverse cipher. Go's `block.Decrypt(dst, src)`, with the block
returned by value.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the block to decrypt |

## Return value

The decrypted block.

## Complexity

Constant, and in constant time: nothing depends on the key or the block but values.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // FIPS 197, appendix C.3: the ciphertext under the key of 256 bits
    vector<byte> key = encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> input = encoding::hex::decode("8ea2b7ca516745bfeafc49904b496089");
    array<byte, 16> block;
    for (int i : range(16)) {
        block[i] = input[i];
    }
    crypto::aes cipher(key);
    println("{}", encoding::hex::encode(cipher.decrypt_block(block)));
}
```

Output:

```text
00112233445566778899aabbccddeeff
```

## See also

- [encrypt_block](encrypt_block.md): encrypts one block
- [sgcl::crypto::aes](../aes.md)
