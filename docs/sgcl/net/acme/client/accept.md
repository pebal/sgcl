[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::accept, async_accept

```cpp
expected<acme::challenge, io::error> accept(const acme::challenge& c) const;                         // (1)
async::task<expected<acme::challenge, io::error>> async_accept(acme::challenge c) const noexcept;    // (2)
```

The challenge answered (RFC 8555 §7.5.1): the CA is told to validate it now, so the response must already be served
(http-01, tls-alpn-01) or the record published (dns-01). The CA validates in the background; the result is what
[wait_authorization](wait_authorization.md) waits for.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | a challenge of an authorization |

## Return value

The challenge as the CA answered (`processing`, or already `valid` or `invalid`), or the error.

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
    net::acme::challenge answered = acme.accept(az.challenges[0]);
    println("{} {}", answered.type, net::acme::to_string(answered.status));
}
```

Output:

```text
http-01 valid
```

## See also

- [wait_authorization](wait_authorization.md), [key_authorization](key_authorization.md)
- [sgcl::net::acme::client](README.md)
