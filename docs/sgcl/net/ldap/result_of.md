[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::result_of

```cpp
optional<result> result_of(const io::error& e) noexcept;
```

The server's [result](result.md) an error of an operation carries: the code, the matched DN, the message, the
referrals. None for an error of another category or of the client's own ([errc](errc.md) `malformed` and
`invalid_filter`).

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |


## Return value

The result, or none.

## Complexity

Linear in the error's path.

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
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    auto r = c.remove("uid=zed,ou=people,dc=example,dc=com");
    auto res = net::ldap::result_of(r.error());
    println("{} {}", res->code, r.error().code().message());
    println("{}", bool(net::ldap::result_of(io::error(io::errc::closed, "ldap", ""))));
}
```

Output:

```text
32 no such object
false
```

## See also

- [result](result.md)
- [errc](errc.md)
