[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::authority_key_id

```cpp
const vector<byte>& authority_key_id() const noexcept;
```

Returns the keyIdentifier of the authorityKeyIdentifier: the [subject_key_id](subject_key_id.md) of the issuer's key.
A chain is built on the names; the key identifiers only order the parents that have the issuer's name (a parent whose
subjectKeyIdentifier is the child's authorityKeyIdentifier is tried first, one whose is not is still tried, as Go has
it; OpenSSL does not take it as the issuer).

## Parameters

None.

## Return value

The identifier, empty when there is none.

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

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", encoding::hex::encode(cert.authority_key_id()));
    println("{}", cert.authority_key_id() == root.subject_key_id());
}
```

Output:

```text
a09a230dac6877136ec4c3e8460c0b7e13b85f11
true
```

## See also

- [subject_key_id](subject_key_id.md)
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
