[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::lookup

```cpp
optional<revoked_certificate> lookup(const slice<const byte>& serial) const noexcept;    // (1)
optional<revoked_certificate> lookup(const certificate& cert) const noexcept;            // (2)
```

1. Returns the entry of the serial number, its INTEGER's bytes as [certificate::serial_number](../x509-certificate/serial_number.md) has them.
2. The same of the certificate's serial number.

- (1–2) The list is not verified, nor whether the certificate is of its issuer: [status_of](status_of.md) does both.

## Parameters

| Parameter | Description |
|---|---|
| `serial` | the serial number's bytes |
| `cert` | the certificate whose serial number is looked up |

## Return value

The entry, or `nullopt` when the list does not hold the serial number.

## Complexity

Linear in the number of entries: the serials compared in place, nothing allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    auto revoked = crypto::x509::certificate::from_pem(io::read_text(dir + "revoked.pem")).value();
    auto good = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    println("{} {}", crl.lookup(revoked).has_value(), crl.lookup(good).has_value());
}
```

Output:

```text
true false
```

## See also

- [status_of](status_of.md): the status, verified
- [sgcl::crypto::x509::revocation_list](README.md)
