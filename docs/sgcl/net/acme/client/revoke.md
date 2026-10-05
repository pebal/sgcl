[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::revoke, async_revoke

```cpp
expected<void, io::error> revoke(const crypto::x509::certificate& cert,                               // (1)
                                 revocation_reason reason = revocation_reason::unspecified) const;
expected<void, io::error> revoke(const tls::identity& id,                                             // (2)
                                 revocation_reason reason = revocation_reason::unspecified) const;
async::task<expected<void, io::error>>                                                                // (3)
    async_revoke(crypto::x509::certificate cert,
                 revocation_reason reason = revocation_reason::unspecified) const noexcept;
async::task<expected<void, io::error>>                                                                // (4)
    async_revoke(tls::identity id,
                 revocation_reason reason = revocation_reason::unspecified) const noexcept;
```

The certificate revoked (RFC 8555 §7.6), for the [reason](../revocation_reason.md) given (none sent for
`unspecified`).

- (1, 3) Signed by the account: the account that ordered it, or one that holds authorizations for all its names.
- (2, 4) Signed by the certificate's own key, the identity's (its leaf the certificate), its JWK in the request:
  what anyone who holds the key does, the account lost or another's.
- (1–2) Block the calling thread, for a thread of the program, never a worker; (3–4) return a task.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate |
| `id` | the certificate's identity: its leaf and its key |
| `reason` | why ([revocation_reason](../revocation_reason.md)) |

## Return value

Nothing, or the error: `errc::already_revoked`, `errc::bad_revocation_reason`, `errc::unauthorized` for an account
that may not.

## Complexity

One request to the CA, and its waits.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

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
    auto leaf = acme.certificate(done.certificate)->certificates[0];
    acme.revoke(leaf, net::acme::revocation_reason::superseded);
    println("{}", ca.revoked(leaf));
    println("{}", acme.revoke(leaf).error().code() == net::acme::errc::already_revoked);
}
```

Output:

```text
true
true
```

## See also

- [revocation_reason](../revocation_reason.md)
- [sgcl::net::acme::client](README.md)
