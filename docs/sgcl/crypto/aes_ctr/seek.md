[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](../aes_ctr.md)

# sgcl::crypto::aes_ctr::seek

```cpp
void seek(uint64_t block);
```

Moves to the start of block `block` of the keystream: the counter is set to the initial counter plus `block`, a
128-bit addition, so that block *n* of a large file decrypts without the blocks before it. Forwards or backwards;
the part of a block left from the last call is dropped and zeroed. Go's CTR has no seek.

## Parameters

| Parameter | Description |
|---|---|
| `block` | the number of the block, counted from the initial counter, 0 for the start |

## Return value

None.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.2: the last block of the ciphertext decrypted alone
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> block3 = encoding::hex::decode("1e031dda2fbe03d1792170a0f3009cee");
    crypto::aes_ctr ctr(key, iv);
    ctr.seek(3);
    ctr.xor_key_stream(block3, block3);
    println("{}", encoding::hex::encode(block3));

    vector<byte> block0(16);
    ctr.seek(0);  // back to the start: block 0's keystream
    ctr.xor_key_stream(block0, block0);
    println("{}", encoding::hex::encode(block0));
}
```

Output:

```text
f69f2445df4f9b17ad2b417be66c3710
ec8cdf7398607cb0f2d21675ea9ea1e4
```

## See also

- [xor_key_stream](xor_key_stream.md): XORs the keystream into the data
- [sgcl::crypto::aes_ctr](../aes_ctr.md)
