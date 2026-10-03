[sgcl](../../README.md) › [crypto](../README.md) › [chacha20_poly1305](README.md)

# sgcl::crypto::chacha20_poly1305::chacha20_poly1305

```cpp
explicit chacha20_poly1305(const slice<const byte>& key);           // (1)
chacha20_poly1305(chacha20_poly1305&& other) noexcept = default;    // (2)
```

1. Copies the 32 bytes of `key` into the object.
2. Takes the key of `other` over; `other` is overwritten with zeros and holds no key, and any call on it but the
   assignment and the destructor throws `logic_error`.

There is no copy constructor: a copy of a secret is asked for by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | 32 bytes |
| `other` | the object whose key is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `key` is not 32 bytes.
- (2) None.

## Notes

The constructor is for a key whose length the program itself fixes; a key read from a file or a message goes
through [from_key](from_key.md), which gives an error instead of throwing.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.8.2: the first 20 bytes of the ciphertext are the RFC's
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("070000004041424344454647");
    vector<byte> aad = encoding::hex::decode("50515253c0c1c2c3c4c5c6c7");
    string text = "Ladies and Gentlemen";

    crypto::chacha20_poly1305 aead(key);
    crypto::chacha20_poly1305 taken = std::move(aead);
    println("{}", encoding::hex::encode(taken.seal(nonce, text, aad).as_slice(0, 20)));
    try {
        auto sealed = aead.seal(nonce, text, aad);
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
    try {
        crypto::chacha20_poly1305 short_key(key.as_slice(0, 16));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
d31a8d34648e60db7b86afbc53ef7ec2a4aded51
sgcl::crypto::chacha20_poly1305: used after being moved from
sgcl::crypto::chacha20_poly1305: a key of 16 bytes
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy of the key, made on purpose
- [operator=](operator_assign.md): takes another object's key over
- [sgcl::crypto::chacha20_poly1305](README.md)
