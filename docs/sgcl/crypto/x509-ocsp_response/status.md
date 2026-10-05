[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::status

```cpp
ocsp_response_status status() const noexcept;
```

Returns the response's status (RFC 6960 §4.2.1): `successful`, or why the responder gave no status ([ocsp_response_status](../x509-ocsp_response_status.md)).

## Parameters

None.

## Return value

The status.

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
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    unsigned char later[] = {0x30, 0x03, 0x0a, 0x01, 0x03};   // tryLater
    auto busy = crypto::x509::ocsp_response::parse(
        slice<const byte>(reinterpret_cast<const byte*>(later), 5)).value();
    println("{} {}", response.status() == crypto::x509::ocsp_response_status::successful,
            busy.status() == crypto::x509::ocsp_response_status::try_later);
}
```

Output:

```text
true true
```

## See also

- [verify](verify.md): a response that is not successful does not verify
- [sgcl::crypto::x509::ocsp_response](README.md)
