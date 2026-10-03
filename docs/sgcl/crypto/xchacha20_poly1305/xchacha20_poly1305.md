[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](README.md)

# sgcl::crypto::xchacha20_poly1305::xchacha20_poly1305

```cpp
explicit xchacha20_poly1305(const slice<const byte>& key);            // (1)
xchacha20_poly1305(xchacha20_poly1305&& other) noexcept = default;    // (2)
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
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::xchacha20_poly1305 aead(vector<byte>(32));
    crypto::xchacha20_poly1305 taken = std::move(aead);
    string text = "sealed under a random nonce";
    println("{}", string(taken.open_random(taken.seal_random(text))));
    try {
        auto sealed = aead.seal_random(text);
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
    try {
        crypto::xchacha20_poly1305 short_key(vector<byte>(24));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
sealed under a random nonce
sgcl::crypto::xchacha20_poly1305: used after being moved from
sgcl::crypto::xchacha20_poly1305: a key of 24 bytes
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy of the key, made on purpose
- [operator=](operator_assign.md): takes another object's key over
- [sgcl::crypto::xchacha20_poly1305](README.md)
