[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::remove, async_remove

```cpp
expected<void, io::error> remove(const string& dn) const;                         // (1)
async::task<expected<void, io::error>> async_remove(string dn) const noexcept;    // (2)
```

DelRequest (RFC 4511 §4.8): the entry removed. Only a leaf: an entry with children is `errc::not_allowed_on_non_leaf`.

`remove` waits on the calling thread; a task awaits `async_remove`.

## Parameters

| Parameter | Description |
|---|---|
| `dn` | the entry |


## Return value

Nothing; `errc::no_such_object`, `errc::not_allowed_on_non_leaf`, the server's other results.

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
    c.remove("uid=carol,ou=people,dc=example,dc=com").value();
    println("{}", c.search("ou=people,dc=example,dc=com", "(uid=*)")->entries.size());
    auto parent = c.remove("ou=people,dc=example,dc=com");
    println("{}", parent.error().code() == net::ldap::errc::not_allowed_on_non_leaf);
}
```

Output:

```text
2
true
```

## See also

- [add](add.md)
- [client](README.md)
