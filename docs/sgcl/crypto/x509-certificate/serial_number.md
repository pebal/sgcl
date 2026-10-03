[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::serial_number

```cpp
const vector<byte>& serial_number() const noexcept;
```

Returns the serial number as the certificate has it: the INTEGER's bytes, big-endian two's complement in the
shortest form, with a `00` in front of a first byte of `80` or more. A negative number, which RFC 5280 forbids and a
few old roots hold, starts with a set bit. The issuer and the serial number name a certificate in a CRL or an OCSP
request.

## Parameters

None.

## Return value

The bytes of the serial number.

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{}", encoding::hex::encode(cert.serial_number()));
}
```

Output:

```text
5240f7e0fb633fa95ddd351aa6f2d5151a5a3c0c
```

## See also

- [issuer](issuer.md): the name the serial number is unique under
- [sgcl::crypto::x509::certificate](README.md)
