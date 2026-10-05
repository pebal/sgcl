[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::new_order, async_new_order

```cpp
expected<acme::order, io::error> new_order(const vector<string>& names,                                // (1)
                                           const order_options& o = {}) const;
async::task<expected<acme::order, io::error>> async_new_order(vector<string> names,                    // (2)
                                                              order_options o = {}) const noexcept;
```

A new order of a certificate for the names (RFC 8555 §7.4). Each name is made an [identifier](../identifier.md) by the
client: an IP address (RFC 8738) as RFC 5952 writes it, else a DNS name in lower case, without its trailing dot, in
A-labels (a name in Unicode by IDNA), a wildcard's `*.` kept. The order comes `pending` with an authorization per name
— or `ready`, when the CA reuses authorizations the account proved before.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `names` | the names: `"example.com"`, `"*.example.com"`, `"192.0.2.1"` |
| `o` | the validity, the certificate it replaces, a profile ([order_options](../order_options.md)) |

## Return value

The order, or the error: `errc::rejected_identifier` for a name that is not one (before a request) or one the CA will
not issue for, `errc::malformed` for no names, `errc::unsupported` for `replaces` to a CA without renewalInfo,
`errc::already_replaced`, `errc::invalid_profile`, `errc::rate_limited`.

## Complexity

One request to the CA, and its waits.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com", "*.example.com", "192.0.2.1"});
    println("{}, {} authorizations", net::acme::to_string(o.status), o.authorizations.size());
    for (auto& id : o.identifiers) {
        println("{} {}", id.type, id.value);
    }
    println("{}", acme.new_order({"exa mple.com"}).error().message());
}
```

Output:

```text
pending, 3 authorizations
dns example.com
dns *.example.com
ip 192.0.2.1
acme new-order a name with a character a DNS name has not: exa mple.com: acme: the CA will not issue for the identifier
```

## See also

- [order](../order.md), [order_options](../order_options.md)
- [sgcl::net::acme::client](README.md)
