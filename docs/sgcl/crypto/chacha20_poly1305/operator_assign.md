[sgcl](../../README.md) › [crypto](../README.md) › [chacha20_poly1305](README.md)

# sgcl::crypto::chacha20_poly1305::operator=

```cpp
chacha20_poly1305& operator=(chacha20_poly1305&& other) noexcept = default;
```

Takes the key of `other` over, written over the key this object held; `other` is overwritten with zeros and holds
no key. An assignment of an object to itself does nothing. There is no copy assignment: a copy is
[clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the object whose key is taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("070000004041424344454647");
    string text = "Ladies and Gentlemen";

    crypto::chacha20_poly1305 aead(vector<byte>(32));
    crypto::chacha20_poly1305 next(key);
    aead = std::move(next);  // the zero key is overwritten
    auto sealed = aead.seal(nonce, text);
    println("{}", string(crypto::chacha20_poly1305(key).open(nonce, sealed)));
    try {
        auto again = next.clone();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
Ladies and Gentlemen
sgcl::crypto::chacha20_poly1305: used after being moved from
```

## See also

- [(constructor)](chacha20_poly1305.md): takes a key, or another object's over
- [clone](clone.md): a copy of the key, made on purpose
- [sgcl::crypto::chacha20_poly1305](README.md)
