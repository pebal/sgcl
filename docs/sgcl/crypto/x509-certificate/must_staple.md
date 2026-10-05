[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::must_staple

```cpp
bool must_staple() const noexcept;
```

Checks whether the certificate is of OCSP Must-Staple: its TLS feature extension (RFC 7633) names `status_request`
(5), so a server presenting it must staple an OCSP response of it. A client of
[net::tls](../../net/tls/revocation_mode.md) that checks revocation refuses such a server without a staple.

## Parameters

None.

## Return value

`true` when the TLS feature extension lists `status_request`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto staple = crypto::x509::certificate::from_pem(io::read_text(dir + "staple.pem")).value();
    println("{} {}", leaf.must_staple(), staple.must_staple());
}
```

Output:

```text
false true
```

## See also

- [ocsp_servers](ocsp_servers.md): where a server fetches the response it staples
- [sgcl::crypto::x509::certificate](README.md)
