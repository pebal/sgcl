[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::account_key

```cpp
account_key();                                      // (1)
explicit account_key(key_algorithm a);              // (2)
account_key(const account_key& other) = default;    // (3)
```

1. A new P-256 key: ES256.
2. A new key of the algorithm `a` ([key_algorithm](../key_algorithm.md)); an RSA key of 2048 bits.
3. The same key: a copy of the handle.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the algorithm |
| `other` | the key to share |

## Complexity

Constant for the curves; an RSA key's generation is a search for primes, tens of milliseconds.

## Exceptions

- (2) `std::invalid_argument` for an `a` of no value of its enumeration.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key ec;
    net::acme::account_key ed(net::acme::key_algorithm::eddsa);
    net::acme::account_key copy = ec;
    println("{} {} {}", ec.algorithm() == net::acme::key_algorithm::es256, ed == ec, copy == ec);
}
```

Output:

```text
true false true
```

## See also

- [from_pem](from_pem.md): a key that exists
- [sgcl::net::acme::account_key](README.md)
