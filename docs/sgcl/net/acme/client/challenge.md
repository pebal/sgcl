[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::challenge, async_challenge

```cpp
expected<acme::challenge, io::error> challenge(const string& url) const;                         // (1)
async::task<expected<acme::challenge, io::error>> async_challenge(string url) const noexcept;    // (2)
```

The challenge of a URL as it is now: a POST-as-GET (RFC 8555 §7.5.1).

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the challenge's URL |

## Return value

The [challenge](../challenge.md), or the error.

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
    net::acme::challenge first = acme.authorization(o.authorizations[0])->challenges[0];
    acme.accept(first);
    net::acme::challenge now = acme.challenge(first.url);
    println("{} {}", now.type, net::acme::to_string(now.status));
}
```

Output:

```text
http-01 valid
```

## See also

- [accept](accept.md)
- [sgcl::net::acme::client](README.md)
