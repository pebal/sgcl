[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::serial_number

```cpp
const vector<byte>& serial_number() const noexcept;
```

Returns the CertID's serialNumber: the certificate's, the bytes of its INTEGER as [certificate::serial_number](../x509-certificate/serial_number.md) has them.

## Parameters

None.

## Return value

The bytes.

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
    auto request = crypto::x509::ocsp_request::make(leaf, issuer).value();
    println("{}", encoding::hex::encode(request.serial_number()));
}
```

Output:

```text
2001
```

## See also

- [make](make.md): what makes them
- [sgcl::crypto::x509::ocsp_request](README.md)
