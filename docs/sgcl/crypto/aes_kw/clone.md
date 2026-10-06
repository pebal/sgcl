[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::clone

```cpp
aes_kw clone() const;
```

A second object with the same key-encryption key: a copy of a secret, made on purpose and by name.

## Parameters

None.

## Return value

An object with the key of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_kw wrapper(vector<byte>(16));
    crypto::aes_kw copy = wrapper.clone();
    vector<byte> key(16);
    println("{}", wrapper.wrap(key) == copy.wrap(key));
}
```

Output:

```text
true
```

## See also

- [(constructor)](aes_kw.md): sets up a key, or takes another object's over
- [sgcl::crypto::aes_kw](README.md)
