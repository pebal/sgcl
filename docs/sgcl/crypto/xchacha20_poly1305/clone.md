[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](../xchacha20_poly1305.md)

# sgcl::crypto::xchacha20_poly1305::clone

```cpp
xchacha20_poly1305 clone() const;
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
    crypto::xchacha20_poly1305 aead(vector<byte>(32));
    crypto::xchacha20_poly1305 copy = aead.clone();
    string text = "the same key";
    auto sealed = aead.seal_random(text);
    println("{}", string(copy.open_random(sealed)));
}
```

Output:

```text
the same key
```

## See also

- [(constructor)](xchacha20_poly1305.md): takes a key, or another object's over
- [sgcl::crypto::xchacha20_poly1305](../xchacha20_poly1305.md)
