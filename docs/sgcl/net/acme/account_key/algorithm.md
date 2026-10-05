[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::algorithm

```cpp
key_algorithm algorithm() const noexcept;
```

The JWS algorithm the key signs with ([key_algorithm](../key_algorithm.md)), by its kind: ES256 for P-256, ES384 for
P-384, EdDSA for Ed25519, RS256 for RSA.

## Parameters

None.

## Return value

The algorithm.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key(net::acme::key_algorithm::es384);
    println("{}", key.algorithm() == net::acme::key_algorithm::es384);
}
```

Output:

```text
true
```

## See also

- [sgcl::net::acme::account_key](README.md)
