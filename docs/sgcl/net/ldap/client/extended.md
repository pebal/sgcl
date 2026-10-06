[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::extended, async_extended

```cpp
expected<pair<string, vector<byte>>, io::error> extended(const string& oid,                                             // (1)
                                                         const slice<const byte>& value = {}) const;
async::task<expected<pair<string, vector<byte>>, io::error>> async_extended(string oid,                                 // (2)
                                                                            vector<byte> value = {}) const noexcept;
```

ExtendedRequest (RFC 4511 §4.12): the operation of the OID with its value, as the operation defines it (BER
mostly); the response's name and value. [who_am_i](who_am_i.md) and [start_tls](start_tls.md) are two of them
with their own functions.

`extended` waits on the calling thread; a task awaits `async_extended`.

## Parameters

| Parameter | Description |
|---|---|
| `oid` | the operation: "1.3.6.1.4.1.4203.1.11.3" |
| `value` | its request value; empty: none |


## Return value

The response's name (an OID, often empty) and value; the server's refusal as its code (`errc::protocol_error` for an operation it does not know).

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
    auto r = c.extended("1.3.6.1.4.1.4203.1.11.3").value();  // Who am I? (RFC 4532)
    println("{}", string(r.second));
}
```

Output:

```text
dn:cn=admin,dc=example,dc=com
```

## See also

- [who_am_i](who_am_i.md)
- [client](README.md)
