[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::allows

```cpp
bool allows(x509::key_usage u) const noexcept;
```

Checks whether the keyUsage has every bit of `u` set. A certificate without a keyUsage allows nothing here; whether
it may sign certificates is [verify](verify.md)'s question, which lets a CA without a keyUsage sign.

## Parameters

| Parameter | Description |
|---|---|
| `u` | the bits asked |

## Return value

`true` when every bit of `u` is in the keyUsage, `false` otherwise.

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", cert.allows(crypto::x509::key_usage::digital_signature));
    println("{}", cert.allows(crypto::x509::key_usage::cert_sign));
    auto both = crypto::x509::key_usage::cert_sign | crypto::x509::key_usage::crl_sign;
    println("{}", root.allows(both));
}
```

Output:

```text
true
false
true
```

## See also

- [key_usage](key_usage.md): every bit
- [sgcl::crypto::x509::certificate](README.md)
