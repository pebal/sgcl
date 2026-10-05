[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::from_pem

```cpp
[[nodiscard]] static expected<certificate_request, error> from_pem(const string& text) noexcept;
```

The first CERTIFICATE REQUEST block of a PEM text (RFC 7468), or NEW CERTIFICATE REQUEST as older tools label it, read
by [parse](parse.md); the text around it and the blocks of other types are passed over.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text |

## Return value

The request, or `errc::malformed` for a text without such a block, or the parse's error.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    auto der = vector<byte>(csr.raw().begin(), csr.raw().end());
    string text = encoding::pem("CERTIFICATE REQUEST", der).to_string();
    auto read = crypto::x509::certificate_request::from_pem("a request:\n" + text);
    println("{} {}", read->dns_names()[0], *read == csr);
}
```

Output:

```text
example.com true
```

## See also

- [parse](parse.md): the request of DER
- [sgcl::crypto::x509::certificate_request](README.md)
