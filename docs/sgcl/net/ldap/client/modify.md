[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::modify, async_modify

```cpp
expected<void, io::error> modify(const string& dn, const vector<modification>& changes) const;                  // (1)
async::task<expected<void, io::error>> async_modify(string dn, vector<modification> changes) const noexcept;    // (2)
```

ModifyRequest (RFC 4511 §4.6): the changes made to the entry in their order, all of them or none: values added,
removed (all of them when none are named), replaced, an integer incremented (RFC 4525).

`modify` waits on the calling thread; a task awaits `async_modify`.

## Parameters

| Parameter | Description |
|---|---|
| `dn` | the entry |
| `changes` | the [modifications](../modification.md), in their order |


## Return value

Nothing; `errc::no_such_attribute`, `errc::attribute_or_value_exists`, `errc::no_such_object`, the server's other results.

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
    c.modify("uid=bob,ou=people,dc=example,dc=com",
             {{net::ldap::modify_op::replace, "title", {"Director"}},
              {net::ldap::modify_op::add, "telephoneNumber", {"+1 555 0100"}}}).value();
    auto bob = c.search("ou=people,dc=example,dc=com", "(uid=bob)")->entries[0];
    println("{} {}", bob.get("title"), bob.get("telephoneNumber"));
}
```

Output:

```text
Director +1 555 0100
```

## See also

- [modification](../modification.md)
- [modify_op](../modify_op.md)
- [client](README.md)
