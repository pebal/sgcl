[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::not_after

```cpp
time::datetime not_after() const noexcept;
```

Returns the last instant at which the certificate is valid: the last second of the period is valid, as Go has it
(OpenSSL ends the period there).

The validity is in UTC. A time the certificate holds outside the years of [time::datetime](../../time/datetime/README.md),
1678 to 2261 (the common `99991231235959Z`, "no expiry"), is the end of that range here, while a verification compares
the time as the certificate has it. Both ends of the period are valid, as RFC 5280 §4.1.2.5 has it.

## Parameters

None.

## Return value

The end of the validity, in UTC.

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

    println("{}", cert.not_after());
    println("{} days", (cert.not_after().unix() - cert.not_before().unix()) / 86400);
}
```

Output:

```text
2126-09-03T17:28:55Z
36500 days
```

## See also

- [not_before](not_before.md): the start of the validity
- [sgcl::crypto::x509::certificate](README.md)
