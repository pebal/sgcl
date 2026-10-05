[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::tls_alpn01_identity

```cpp
expected<tls::identity, io::error> tls_alpn01_identity(const string& token,
                                                       const string& name) const;
```

The identity tls-alpn-01 serves for the name (RFC 8737 §3): a self-signed certificate on a new P-256 key, for the name
alone (an IP address as its iPAddress, RFC 8738 §6), with the critical acmeIdentifier extension holding the SHA-256 of
the [key authorization](key_authorization.md). A TLS server on port 443 serves it to a client that offers `acme-tls/1`
alone, negotiating that protocol: [tls::config](../../tls/config.md)'s `identity_for`, with `acme-tls/1` in its `alpn`.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the challenge's token |
| `name` | the authorization's identifier |

## Return value

The identity, or the error of its making (none for a name of a certificate the module writes).

## Complexity

One P-256 key made and one signature.

## Exceptions

`std::invalid_argument` for a name that is not ASCII (an IDN goes as its A-label).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::client acme("https://ca.example/directory", net::acme::account_key());
    net::tls::identity id = acme.tls_alpn01_identity("token-1", "example.com");
    const auto& cert = id.certificates()[0];
    println("{}", cert.dns_names()[0]);
    for (auto& e : cert.extensions()) {
        if (e.oid == "1.3.6.1.5.5.7.1.31") {
            println("acmeIdentifier, critical {}, {} bytes", e.critical, e.value.size());
        }
    }
}
```

Output:

```text
example.com
acmeIdentifier, critical true, 34 bytes
```

## See also

- [tls::client_hello](../../tls/client_hello.md): an identity chosen per hello
- [sgcl::net::acme::client](README.md)
