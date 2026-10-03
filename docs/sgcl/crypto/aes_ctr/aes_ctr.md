[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](../aes_ctr.md)

# sgcl::crypto::aes_ctr::aes_ctr

```cpp
aes_ctr(const slice<const byte>& key, const slice<const byte>& iv);    // (1)
aes_ctr(aes_ctr&& other) noexcept;                                     // (2)
aes_ctr(const aes_ctr&) = delete;                                      // (3)
```

1. Sets up the key schedule of `key`, AES-128, AES-192 or AES-256 by its length, and the counter at `iv`; the
   keystream starts at block 0, the encryption of `iv`.
2. Takes the state of `other` over, its place in the keystream with it; `other` is overwritten with zeros and holds
   no key, its `key_size()` 0, and any call on it but the assignment and the destructor throws `logic_error`.
3. The state is a secret and is not copied by accident: a copy is asked for by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | 16, 24 or 32 bytes |
| `iv` | the initial counter block, 16 bytes, read as a 128-bit big-endian number |
| `other` | the object whose state is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `key` is not 16, 24 or 32 bytes, or `iv` not 16.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.3 and F.5.5: the first block under keys of 192 and 256 bits
    vector<byte> key192 = encoding::hex::decode("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
    vector<byte> key256 = encoding::hex::decode(
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> block = encoding::hex::decode("6bc1bee22e409f96e93d7e117393172a");
    for (const auto& key : {key192, key256}) {
        crypto::aes_ctr ctr(key, iv);
        ctr.xor_key_stream(block, block);
        println("{}", encoding::hex::encode(block));
        ctr.seek(0);
        ctr.xor_key_stream(block, block);  // back to the plaintext
    }

    try {
        crypto::aes_ctr wrong(key256, iv.as_slice(0, 12));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
1abc932417521ca24f2b0459fe7e6e0b
601ec313775789a5b7a7f504bbf3d228
sgcl::crypto::aes_ctr: an initial counter of 12 bytes, not 16
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy that goes on from the same place
- [operator=](operator_assign.md): takes another object's state over
- [sgcl::crypto::aes_ctr](../aes_ctr.md)
