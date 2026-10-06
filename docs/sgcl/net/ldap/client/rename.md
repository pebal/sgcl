[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::rename, async_rename

```cpp
expected<void, io::error> rename(const string& dn, const string& new_rdn, bool delete_old_rdn = true,         // (1)
                                 const string& new_superior = {}) const;
async::task<expected<void, io::error>> async_rename(string dn, string new_rdn, bool delete_old_rdn = true,    // (2)
                                                    string new_superior = {}) const noexcept;
```

ModifyDNRequest (RFC 4511 §4.9): the entry renamed to the new RDN, the old RDN's value dropped from its attributes
or kept, and moved under another parent when one is named.

`rename` waits on the calling thread; a task awaits `async_rename`.

## Parameters

| Parameter | Description |
|---|---|
| `dn` | the entry |
| `new_rdn` | its new relative name: "uid=alicia" |
| `delete_old_rdn` | whether the old RDN's value leaves the attributes |
| `new_superior` | the new parent; empty: the same |


## Return value

Nothing; `errc::no_such_object`, `errc::entry_already_exists`, the server's other results.

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
    c.rename("uid=alice,ou=people,dc=example,dc=com", "uid=alicia").value();
    auto r = c.search("ou=people,dc=example,dc=com", "(uid=alicia)").value();
    println("{} {}", r.entries[0].dn, r.entries[0].get_all("uid").size());
    c.rename("uid=alicia,ou=people,dc=example,dc=com", "uid=alicia", true, "ou=groups,dc=example,dc=com");
    println("{}", c.search("dc=example,dc=com", "(uid=alicia)")->entries[0].dn);
}
```

Output:

```text
uid=alicia,ou=people,dc=example,dc=com 1
uid=alicia,ou=groups,dc=example,dc=com
```

## See also

- [modify](modify.md)
- [client](README.md)
