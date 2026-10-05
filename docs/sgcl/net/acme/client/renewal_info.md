[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::renewal_info, async_renewal_info

```cpp
expected<acme::renewal_info, io::error> renewal_info(const crypto::x509::certificate& cert) const;    // (1)
async::task<expected<acme::renewal_info, io::error>>                                                  // (2)
    async_renewal_info(crypto::x509::certificate cert) const noexcept;
```

When the CA suggests the certificate be renewed (RFC 9773): an unauthenticated GET of renewalInfo and the
certificate's [renewal_id](renewal_id.md), the window of the answer and its Retry-After. A renewal at a point of the
window chosen at random, by an order whose `replaces` is the certificate's id, is what RFC 9773 asks; the
[manager](../manager/README.md) does it.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | a certificate of the CA |

## Return value

The [renewal_info](../renewal_info.md), or the error: `errc::unsupported` for a CA without renewalInfo,
`errc::malformed` for a certificate without an authorityKeyIdentifier.

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

// An order of the name taken to its certificate (the test server's challenges
// are valid as answered)
net::acme::order issued(const net::acme::client& acme, const string& name) {
    net::acme::order o = acme.new_order({name});
    acme.accept(acme.authorization(o.authorizations[0])->challenges[0]);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {name};
    return acme.finalize(acme.wait_order(o.url), crypto::x509::create_certificate_request(t, key));
}

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    auto leaf = acme.certificate(issued(acme, "example.com").certificate)->certificates[0];
    net::acme::renewal_info info = acme.renewal_info(leaf);
    println("{}", info.start.unix() < info.end.unix() && info.end.unix() < leaf.not_after().unix());
    auto id = net::acme::client::renewal_id(leaf);
    net::acme::order renewal = acme.new_order({"example.com"}, {.replaces = *id});
    println("{}", renewal.replaces == *id);
}
```

Output:

```text
true
true
```

## See also

- [renewal_id](renewal_id.md), [renewal_info](../renewal_info.md)
- [sgcl::net::acme::client](README.md)
