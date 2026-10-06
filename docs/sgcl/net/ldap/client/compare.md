[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::compare, async_compare

```cpp
expected<bool, io::error> compare(const string& dn, const string& attribute, const string& value) const;           // (1)
async::task<expected<bool, io::error>> async_compare(string dn, string attribute, string value) const noexcept;    // (2)
```

CompareRequest (RFC 4511 §4.10): whether the entry's attribute has the value, by the attribute's own equality (a
`cn` without case). A password checked this way never leaves the server.

`compare` waits on the calling thread; a task awaits `async_compare`.

## Parameters

| Parameter | Description |
|---|---|
| `dn` | the entry |
| `attribute` | the attribute |
| `value` | the value |


## Return value

Whether it has it; `errc::no_such_object`, `errc::no_such_attribute` where the server says so, the server's other results.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    println("{}", c.compare("uid=alice,ou=people,dc=example,dc=com", "title", "engineer").value());
    println("{}", c.compare("uid=alice,ou=people,dc=example,dc=com", "title", "Manager").value());
}
```

Output:

```text
true
false
```

## See also

- [search](search.md)
- [client](README.md)
