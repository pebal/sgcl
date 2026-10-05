[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::operator==, operator!= (sgcl::net::acme::account_key)

```cpp
friend bool operator==(const account_key& a, const account_key& b) noexcept;
```

Whether two handles are of the same key: the same handle, or keys of the same public half (their [jwk](jwk.md)s). `!=`
is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same key.

## Complexity

Linear in the length of the JWK; constant for copies.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key a;
    auto again = net::acme::account_key::from_pem(a.to_pem());
    println("{} {}", a == *again, a != net::acme::account_key());
}
```

Output:

```text
true true
```

## See also

- [sgcl::net::acme::account_key](README.md)
