[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::wait_authorization, async_wait_authorization

```cpp
expected<acme::authorization, io::error> wait_authorization(const string& url) const;    // (1)
async::task<expected<acme::authorization, io::error>>                                    // (2)
    async_wait_authorization(string url) const noexcept;
```

The authorization polled until it is no longer `pending` (RFC 8555 §7.5.1), after its challenge was
[accept](accept.md)ed: the polls go at the CA's Retry-After, else every `options::poll_interval`.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the authorization's URL |

## Return value

The authorization, `valid`; or `errc::authorization_invalid` for one that ends otherwise, the identifier, the status
and every failed challenge's problem in the text; `ETIMEDOUT` past `options::poll_timeout`.

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
    net::acme::test_server ca;   // validates for real: nothing answers the challenge here
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com"});
    net::acme::authorization az = acme.authorization(o.authorizations[0]);
    for (auto& c : az.challenges) {
        if (c.type == "dns-01") {
            acme.accept(c);
        }
    }
    auto failed = acme.wait_authorization(az.url);
    println("{}", failed.error().message());
}
```

Output:

```text
acme authorization example.com: the authorization is invalid; dns-01: no TXT record of _acme-challenge.example.com: acme: the authorization is invalid
```

## See also

- [accept](accept.md)
- [sgcl::net::acme::client](README.md)
