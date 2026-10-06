[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::unbind, async_unbind

```cpp
expected<void, io::error> unbind() const;                                // (1)
async::task<expected<void, io::error>> async_unbind() const noexcept;    // (2)
```

UnbindRequest (RFC 4511 §4.3), then the connection closed: the end of the session. The operations still waiting fail.

`unbind` waits on the calling thread; a task awaits `async_unbind`.

## Parameters

None.

## Return value

Nothing; `io::errc::closed` for a session ended already.

## Complexity

Constant.

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
    println("{}", bool(c.unbind()));
    println("{}", c.who_am_i().error().code() == io::errc::closed);
}
```

Output:

```text
true
true
```

## See also

- [close](close.md)
- [client](README.md)
