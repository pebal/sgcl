[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::clone

```cpp
private_key clone() const;
```

Makes a second key with the same numbers, in a block of its own that is zeroed when that key goes. The key is
move-only: a copy of a secret is made by this name, never by an `=` that could be written without a thought.

## Parameters

None.

## Return value

A key of its own, equal to this one.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto second = key.clone();
    println("{}", second.public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [(constructor)](rsa-private_key.md): the move
- [sgcl::crypto::rsa::private_key](README.md)
