[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::authorization, async_authorization

```cpp
expected<acme::authorization, io::error> authorization(const string& url) const;    // (1)
async::task<expected<acme::authorization, io::error>>                               // (2)
    async_authorization(string url) const noexcept;
```

The authorization of a URL as it is now (RFC 8555 §7.5): its identifier, its status, its challenges.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | an order's authorization URL |

## Return value

The [authorization](../authorization.md), or the error.

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
    net::acme::order o = acme.new_order({"example.com"});
    net::acme::authorization az = acme.authorization(o.authorizations[0]);
    println("{} {} {}", az.identifier.value, net::acme::to_string(az.status), az.challenges.size());
}
```

Output:

```text
example.com pending 3
```

## See also

- [wait_authorization](wait_authorization.md)
- [sgcl::net::acme::client](README.md)
