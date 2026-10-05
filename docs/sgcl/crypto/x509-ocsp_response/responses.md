[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::responses

```cpp
const vector<ocsp_single_response>& responses() const noexcept;
```

Returns every SingleResponse of the response, in order: a status of a certificate each, named by its CertID ([ocsp_single_response](../x509-ocsp_single_response.md)). They are as the responder wrote them, not verified; [verify](verify.md) gives the one of a certificate, verified.

## Parameters

None.

## Return value

The single responses; empty for a response that is not successful.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    for (const auto& single : response.responses()) {
        bool good = single.status == crypto::x509::revocation_status::good;
        println("{} {}", encoding::hex::encode(single.serial_number), good);
    }
}
```

Output:

```text
2001 true
```

## See also

- [verify](verify.md): the one of a certificate, verified
- [sgcl::crypto::x509::ocsp_response](README.md)
