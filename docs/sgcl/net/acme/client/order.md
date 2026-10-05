[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::order, async_order

```cpp
expected<acme::order, io::error> order(const string& url) const;                         // (1)
async::task<expected<acme::order, io::error>> async_order(string url) const noexcept;    // (2)
```

The order of a URL as it is now: a POST-as-GET (RFC 8555 §7.4), with the CA's Retry-After in `retry_after` while it is
processing.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the order's URL |

## Return value

The order, or the error: a problem of the CA for an order not the account's.

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
    acme.accept(acme.authorization(o.authorizations[0])->challenges[0]);
    println("{}", net::acme::to_string(acme.order(o.url)->status));
}
```

Output:

```text
ready
```

## See also

- [wait_order](wait_order.md)
- [sgcl::net::acme::client](README.md)
