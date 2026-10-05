[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::wait_order, async_wait_order

```cpp
expected<acme::order, io::error> wait_order(const string& url) const;                         // (1)
async::task<expected<acme::order, io::error>> async_wait_order(string url) const noexcept;    // (2)
```

The order polled until it is no longer `pending` or `processing` (RFC 8555 §7.4): `ready` after its authorizations,
`valid` after its finalization. The polls go at the CA's Retry-After, else every `options::poll_interval`.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the order's URL |

## Return value

The order, `ready` or `valid`; or `errc::order_invalid` for one that ends `invalid` (its problem's detail in the text),
`ETIMEDOUT` past `options::poll_timeout`, the error of a request.

## Complexity

A request per poll, and the pauses between.

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
    println("{}", net::acme::to_string(acme.wait_order(o.url)->status));
    net::acme::order other = acme.new_order({"other.example"});
    acme.deactivate_authorization(other.authorizations[0]);
    println("{}", acme.wait_order(other.url).error().code() == net::acme::errc::order_invalid);
}
```

Output:

```text
ready
true
```

## See also

- [order](order.md), [finalize](finalize.md)
- [sgcl::net::acme::client](README.md)
