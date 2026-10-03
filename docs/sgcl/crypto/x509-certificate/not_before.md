[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::not_before

```cpp
time::datetime not_before() const noexcept;
```

Returns the first instant at which the certificate is valid.

The validity is in UTC. A time the certificate holds outside the years of [time::datetime](../../time/datetime/README.md),
1678 to 2261 (the common `99991231235959Z`, "no expiry"), is the end of that range here, while a verification compares
the time as the certificate has it. Both ends of the period are valid, as RFC 5280 §4.1.2.5 has it.

## Parameters

None.

## Return value

The start of the validity, in UTC.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{}", cert.not_before());
    println("{}", cert.not_before().unix());
}
```

Output:

```text
2026-09-27T17:28:55Z
1790530135
```

## See also

- [not_after](not_after.md): the end of the validity
- [verify_options](../x509-verify_options.md): the time a verification asks for
- [sgcl::crypto::x509::certificate](README.md)
