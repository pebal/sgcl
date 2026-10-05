[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::deactivate_authorization, async_deactivate_authorization

```cpp
expected<acme::authorization, io::error> deactivate_authorization(const string& url) const;    // (1)
async::task<expected<acme::authorization, io::error>>                                          // (2)
    async_deactivate_authorization(string url) const noexcept;
```

The authorization deactivated (RFC 8555 §7.5.2): what proved control of a name the holder gives up, a `pending` one so
that its order is dropped, a `valid` one so that it is not reused.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the authorization's URL |

## Return value

The authorization, `deactivated`, or the error.

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
    net::acme::authorization az = acme.deactivate_authorization(o.authorizations[0]);
    net::acme::order now = acme.order(o.url);
    println("{} {}", net::acme::to_string(az.status), net::acme::to_string(now.status));
}
```

Output:

```text
deactivated invalid
```

## See also

- [authorization](authorization.md)
- [sgcl::net::acme::client](README.md)
