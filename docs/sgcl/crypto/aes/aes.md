[sgcl](../../README.md) › [crypto](../README.md) › [aes](../aes.md)

# sgcl::crypto::aes::aes

```cpp
/*(1)*/ explicit aes(const slice<const byte>& key);
/*(2)*/ aes(aes&& other) noexcept;
/*(3)*/ aes(const aes&) = delete;
```

1. Sets up the key schedule of `key`: AES-128, AES-192 or AES-256 by its length, the round keys of encryption and
   of decryption, in the object. The path, the processor's instructions or the portable code, is chosen here, from
   the processor alone.
2. Takes the key schedule of `other` over; `other` is overwritten with zeros and holds no key, its `key_size()` 0,
   and any call on it but the assignment and the destructor throws `logic_error`.
3. The key schedule is a secret and is not copied by accident: a copy is asked for by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | 16, 24 or 32 bytes |
| `other` | the object whose key schedule is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `key` is not 16, 24 or 32 bytes.
- (2) None.

## Notes

The constructor is for a key whose length the program itself fixes; a key read from a file or a message goes
through [from_key](from_key.md), which gives an error instead of throwing.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // FIPS 197, appendix C: the same block under keys of 128, 192 and 256 bits
    vector<byte> key = encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    array<byte, 16> block;
    for (int i : range(16)) {
        block[i] = byte(i * 0x11);
    }
    for (size_t n : {16, 24, 32}) {
        crypto::aes cipher(key.as_slice(0, n));
        println("{} {}", n, encoding::hex::encode(cipher.encrypt_block(block)));
    }

    try {
        crypto::aes wrong(key.as_slice(0, 8));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
16 69c4e0d86a7b0430d8cdb78070b4c55a
24 dda97ca4864cdfe06eaf70a0ec0d7191
32 8ea2b7ca516745bfeafc49904b496089
sgcl::crypto::aes: a key of 8 bytes
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy of the key schedule, made on purpose
- [operator=](operator_assign.md): takes another object's key over
- [sgcl::crypto::aes](../aes.md)
