[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::certificate, async_certificate

```cpp
expected<tls::identity, io::error> certificate(const string& name) const;                         // (1)
async::task<expected<tls::identity, io::error>> async_certificate(string name) const noexcept;    // (2)
```

The identity of a name, what each handshake of [tls_config](tls_config.md) gets: the certificate of the name (or of the
wildcard among the names one label above it), from memory when it holds one that has not expired, else from the cache,
else from the CA — one order at a time per name, the other callers waiting for it — and its renewal started. A name
neither the names nor the host policy allow is refused before a request; a failure is kept for its backoff.

1. Blocks the calling thread: for a thread of the program, never a worker. What warms the certificates before a
   server starts.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, in any case, a trailing dot ignored |

## Return value

The identity, or the error: `errc::host_not_allowed`, `io::errc::closed` after [close](close.md), the error of the
attempt (and, within its backoff, the same error again).

## Complexity

From memory, a lookup; else a file read, or a whole order.

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
    net::acme::manager certificates({"example.com", "www.example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});

    net::tls::identity a = certificates.certificate("Example.COM.");
    net::tls::identity b = certificates.certificate("example.com");   // from memory
    const auto& chain = a.certificates();
    println("{} {} {}", chain[0].dns_names()[0], chain[0] == b.certificates()[0], ca.orders());
    println("{}", certificates.certificate("example.net").error().message());
    certificates.close();
}
```

Output:

```text
example.com true 1
acme example.net: acme: the host is not allowed
```

## See also

- [tls_config](tls_config.md)
- [sgcl::net::acme::manager](README.md)
