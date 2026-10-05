[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::certificate, async_certificate

```cpp
expected<certificate_chain, io::error> certificate(const string& url) const;                         // (1)
async::task<expected<certificate_chain, io::error>> async_certificate(string url) const noexcept;    // (2)
```

The chain of a certificate URL (RFC 8555 §7.4.2): `application/pem-certificate-chain`, the leaf first, each
certificate read; the URLs of the other chains of the same certificate the CA offers (Link `rel="alternate"`), each
one call of this function away.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | an order's `certificate` URL, or an alternate's |

## Return value

The [certificate_chain](../certificate_chain.md), or the error: `errc::malformed_response` for a body that is not a
chain of certificates.

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
    net::acme::order done = issued(acme, "example.com");
    net::acme::certificate_chain chain = acme.certificate(done.certificate);
    println("{}, {} certificates", chain.certificates[0].dns_names()[0], chain.certificates.size());
}
```

Output:

```text
example.com, 2 certificates
```

## See also

- [certificate_chain](../certificate_chain.md)
- [sgcl::net::acme::client](README.md)
