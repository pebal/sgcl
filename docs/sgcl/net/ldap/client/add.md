[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::add, async_add

```cpp
expected<void, io::error> add(const entry& e) const;                         // (1)
async::task<expected<void, io::error>> async_add(entry e) const noexcept;    // (2)
```

AddRequest (RFC 4511 §4.7): the entry made, with its attributes; its parent must exist, its object classes
allow its attributes.

`add` waits on the calling thread; a task awaits `async_add`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry: its DN and attributes |


## Return value

Nothing; `errc::entry_already_exists`, `errc::object_class_violation`, `errc::no_such_object` for a missing parent, the server's other results.

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
    net::ldap::entry dave;
    dave.dn = "uid=dave,ou=people,dc=example,dc=com";
    dave.attributes = {{"objectClass", {"inetOrgPerson"}}, {"uid", {"dave"}},
                       {"cn", {"Dave Bowman"}}, {"sn", {"Bowman"}}};
    c.add(dave).value();
    println("{}", c.search("ou=people,dc=example,dc=com", "(uid=dave)")->entries[0].get("cn"));
    println("{}", c.add(dave).error().code() == net::ldap::errc::entry_already_exists);
}
```

Output:

```text
Dave Bowman
true
```

## See also

- [entry](../entry/README.md)
- [remove](remove.md)
- [client](README.md)
