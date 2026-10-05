[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::finalize, async_finalize

```cpp
expected<acme::order, io::error> finalize(const acme::order& o,                                   // (1)
                                          const crypto::x509::certificate_request& csr) const;
async::task<expected<acme::order, io::error>>                                                     // (2)
    async_finalize(acme::order o, crypto::x509::certificate_request csr) const noexcept;
```

The order finalized with the CSR (RFC 8555 §7.4): the request sent to its finalize URL, the order polled until it is
`valid`, its `certificate` URL set. The CSR is of the certificate's own key, never the account's, and asks for the
order's identifiers, no more and no fewer ([create_certificate_request](../../../crypto/x509-create_certificate_request.md)).

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | a `ready` order |
| `csr` | the certificate request |

## Return value

The order, `valid`; or the error: `errc::order_not_ready` for an order not `ready`, `errc::bad_csr` for a CSR of other
names or of the account's key, `errc::order_invalid`, `ETIMEDOUT` past `options::poll_timeout`.

## Complexity

The request, then a request per poll.

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
    net::acme::order ready = acme.wait_order(o.url);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.org"};
    auto refused = acme.finalize(ready, crypto::x509::create_certificate_request(t, key));
    println("{}", refused.error().code() == net::acme::errc::bad_csr);
    t.dns_names = {"example.com"};
    net::acme::order done = acme.finalize(ready, crypto::x509::create_certificate_request(t, key));
    println("{} {}", net::acme::to_string(done.status), done.certificate.empty());
}
```

Output:

```text
true
valid false
```

## See also

- [certificate](certificate.md)
- [crypto::x509::create_certificate_request](../../../crypto/x509-create_certificate_request.md)
- [sgcl::net::acme::client](README.md)
