[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](../xchacha20_poly1305.md)

# sgcl::crypto::xchacha20_poly1305::operator=

```cpp
xchacha20_poly1305& operator=(xchacha20_poly1305&& other) noexcept = default;
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
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(32, byte(7));
    crypto::xchacha20_poly1305 aead(vector<byte>(32));
    crypto::xchacha20_poly1305 next(key);
    aead = std::move(next);  // the zero key is overwritten
    string text = "under the second key";
    auto sealed = aead.seal_random(text);
    println("{}", string(crypto::xchacha20_poly1305(key).open_random(sealed)));
    try {
        auto again = next.clone();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
under the second key
sgcl::crypto::xchacha20_poly1305: used after being moved from
```

## See also

- [(constructor)](xchacha20_poly1305.md): takes a key, or another object's over
- [clone](clone.md): a copy of the key, made on purpose
- [sgcl::crypto::xchacha20_poly1305](../xchacha20_poly1305.md)
