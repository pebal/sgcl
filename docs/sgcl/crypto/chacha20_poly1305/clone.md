[sgcl](../../README.md) › [crypto](../README.md) › [chacha20_poly1305](../chacha20_poly1305.md)

# sgcl::crypto::chacha20_poly1305::clone

```cpp
chacha20_poly1305 clone() const;
```

A second object with the same key: a copy of a secret, made on purpose and by name, where a copy constructor would
make one by accident. The two seal and open alike and are destroyed, and zeroed, each on its own.

## Parameters

None.

## Return value

An object with the key of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

One object seals and opens from any number of threads at once, so a clone is not needed to share a key between
threads; it is for a second owner, an object that outlives this one.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::chacha20_poly1305 aead(vector<byte>(32));
    crypto::chacha20_poly1305 copy = aead.clone();
    vector<byte> nonce(12);
    string text = "the same key";
    auto sealed = aead.seal(nonce, text);
    println("{}", string(copy.open(nonce, sealed)));
}
```

Output:

```text
the same key
```

## See also

- [(constructor)](chacha20_poly1305.md): takes a key, or another object's over
- [sgcl::crypto::chacha20_poly1305](../chacha20_poly1305.md)
