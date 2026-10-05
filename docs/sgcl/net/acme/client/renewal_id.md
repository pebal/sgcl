[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::renewal_id

```cpp
static expected<string, io::error> renewal_id(const crypto::x509::certificate& cert) noexcept;
```

The ARI identifier of a certificate (RFC 9773 §4.1): the key identifier of its authorityKeyIdentifier and the DER of
its serial number, each base64url, joined by a dot. What [renewal_info](renewal_info.md) asks with and
[order_options](../order_options.md)`::replaces` takes.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate |

## Return value

The identifier, or `errc::malformed` for a certificate without an authorityKeyIdentifier (a self-signed one).

## Complexity

Linear in the size of the identifiers.

## Exceptions

None.

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
    string id = *net::acme::client::renewal_id(leaf);
    println("{}", id.view().find('.') != std::string_view::npos);
}
```

Output:

```text
true
```

## See also

- [renewal_info](renewal_info.md)
- [sgcl::net::acme::client](README.md)
