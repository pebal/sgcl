[sgcl](../../README.md) › [crypto](../README.md) › [aes_gcm](../aes_gcm.md)

# sgcl::crypto::aes_gcm::aes_gcm

```cpp
/*(1)*/ explicit aes_gcm(const slice<const byte>& key);
/*(2)*/ aes_gcm(aes_gcm&& other) noexcept;
/*(3)*/ aes_gcm(const aes_gcm&) = delete;
```

1. Sets up `key`: AES-128, AES-192 or AES-256 by its length, its round keys and the powers of GHASH's key, in the
   object. The path, the processor's instructions or the portable code, is chosen here, from the processor alone.
2. Takes the key of `other` over; `other` is overwritten with zeros and holds no key, its `key_size()` 0, and any
   call on it but the assignment and the destructor throws `logic_error`.
3. The key is not copied by accident: a copy of a secret is asked for by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | 16, 24 or 32 bytes |
| `other` | the object whose key is taken over |

## Complexity

Constant: the key schedule and the powers of the hash key.

## Exceptions

- (1) `invalid_argument` when `key` is not 16, 24 or 32 bytes.
- (2) None.

## Notes

The constructor is for a key whose length the program itself fixes; a key read from a file or a message, whose
length is data, goes through [from_key](from_key.md), which gives an error instead of throwing.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The GCM specification's test cases 1, 7 and 13: a zero key, a zero nonce, nothing to seal
    vector<byte> nonce(12);
    vector<byte> nothing;
    for (size_t n : {16, 24, 32}) {
        vector<byte> key(n);
        crypto::aes_gcm gcm(key);
        println("{} {}", gcm.key_size(), encoding::hex::encode(gcm.seal(nonce, nothing)));
    }

    crypto::aes_gcm first(vector<byte>(16));
    crypto::aes_gcm second = std::move(first);
    println("{} {}", first.key_size(), second.key_size());

    try {
        crypto::aes_gcm wrong(vector<byte>(20));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
16 58e2fccefa7e3061367f1d57a4e7455a
24 cd33b28ac773f74ba00ed1f312572435
32 530f8afbc74536b9a963b4f1c4cb738b
0 16
sgcl::crypto::aes_gcm: a key of 20 bytes
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy of the key, made on purpose
- [operator=](operator_assign.md): takes another object's key over
- [sgcl::crypto::aes_gcm](../aes_gcm.md)
