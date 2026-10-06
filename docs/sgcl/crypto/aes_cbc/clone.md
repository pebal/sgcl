[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::clone

```cpp
aes_cbc clone() const;
```

A second object with the same key at the same point of the chain: a copy of a secret, made on purpose and by name.
From there the two go on each on its own.

## Parameters

None.

## Return value

An object with the state of this one.

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
    vector<byte> key(16), iv(16), first(16), a(16), b(16);
    crypto::aes_cbc cbc(key, iv);
    cbc.encrypt_blocks(first, first);
    crypto::aes_cbc copy = cbc.clone();  // after the first block
    cbc.encrypt_blocks(a, a);
    copy.encrypt_blocks(b, b);
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [(constructor)](aes_cbc.md): sets up a key and an IV, or takes another object's over
- [sgcl::crypto::aes_cbc](README.md)
