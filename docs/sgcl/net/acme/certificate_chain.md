[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::certificate_chain

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct certificate_chain {
        string pem;
        crypto::x509::chain certificates;
        vector<string> alternates;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A certificate the CA issued, as RFC 8555 §7.4.2 downloads it: the chain from the leaf to the CA's root in PEM
(`application/pem-certificate-chain`), its certificates read, and the URLs of the other chains of the same certificate
the CA offers (Link `rel="alternate"`), each one [certificate](client/certificate.md) away. With the certificate's key,
`pem` makes a [tls::identity](../tls/identity/README.md).

## Member objects

| Object | Description |
|---|---|
| `pem` | the chain as the CA sent it, the leaf first |
| `certificates` | the chain read ([crypto::x509::chain](../../crypto/x509.md)), the leaf first |
| `alternates` | the URLs of the alternate chains; empty when the CA offers none |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true, .alternate_chains = 1});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com"});
    net::acme::authorization az = acme.authorization(o.authorizations[0]);
    acme.accept(az.challenges[0]);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    net::acme::order done = acme.finalize(acme.wait_order(o.url), csr);
    net::acme::certificate_chain chain = acme.certificate(done.certificate);
    println("{} certificates, {} alternate", chain.certificates.size(), chain.alternates.size());
    const auto& certs = chain.certificates;
    println("{} issued by {}", certs[0].dns_names()[0], certs[1].subject().common_name());
    net::tls::identity id(chain.pem, key.to_pem());
    println("{}", id.certificates().size());
}
```

Output:

```text
2 certificates, 1 alternate
example.com issued by sgcl acme test intermediate
2
```

## See also

- [client::certificate](client/certificate.md)
- [tls::identity](../tls/identity/README.md)
- [net::acme](README.md)
