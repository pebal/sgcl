[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::certificate_request_template

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct certificate_request_template {
        string common_name;
        vector<string> organization;
        vector<string> dns_names;
        vector<x509::ip_address> ip_addresses;
        vector<string> email_addresses;
        vector<string> uris;
        vector<extension> extensions;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

The fields of a certificate request to make (PKCS #10), what
[create_certificate_request](x509-create_certificate_request.md) writes: the subject and the names a certificate is
asked for, Go's `x509.CertificateRequest` used as a template. The names go into an extensionRequest attribute of a
subjectAltName, as every CA reads them (ACME's finalize among them).

## Member objects

| Object | Description |
|---|---|
| `common_name` | the subject's CN; empty: none (ACME ignores the subject) |
| `organization` | the subject's O |
| `dns_names` | the DNS names asked for, in ASCII (an IDN as its A-label) |
| `ip_addresses` | the IP addresses asked for, 4 or 16 bytes each |
| `email_addresses` | the email addresses asked for |
| `uris` | the URIs asked for |
| `extensions` | more extensions asked for; one of the subjectAltName's OID replaces the module's |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.common_name = "example.com";
    t.dns_names = {"example.com", "www.example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    println("{}: {} names", csr.subject().common_name(), csr.dns_names().size());
}
```

Output:

```text
example.com: 2 names
```

## See also

- [create_certificate_request](x509-create_certificate_request.md): the request of a template
- [certificate_request](x509-certificate_request/README.md): a request read
- [x509](x509.md)
