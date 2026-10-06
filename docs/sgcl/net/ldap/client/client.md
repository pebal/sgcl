[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::client

```cpp
client() noexcept;                       // (1)
client(const client& other) noexcept;    // (2)
```

1. No session: `operator bool` is false; an operation on it is a contract violation.
2. The same session as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    net::ldap::client none;
    println("{}", bool(none));
    net::ldap::client same = c;
    println("{} {}", bool(same), same == c);
}
```

Output:

```text
false
true true
```

## See also

- [connect](connect.md)
- [client](README.md)
