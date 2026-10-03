[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::key_usage

```cpp
x509::key_usage key_usage() const noexcept;
```

Returns the bits of the keyUsage (RFC 5280 §4.2.1.3) as flags, none when there is no keyUsage. The keyUsage of a leaf
is not checked by a verification (Go does not either); a CA's keyCertSign is.

## Parameters

None.

## Return value

The [key_usage](../x509-key_usage.md) flags.

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
    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    auto both = crypto::x509::key_usage::cert_sign | crypto::x509::key_usage::crl_sign;
    println("{}", root.key_usage() == both);
}
```

Output:

```text
true
```

## See also

- [allows](allows.md): checks some of the bits
- [has_key_usage](has_key_usage.md)
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
