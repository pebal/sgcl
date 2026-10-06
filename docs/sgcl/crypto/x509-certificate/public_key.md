[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::public_key

```cpp
const x509::public_key& public_key() const noexcept;
```

Returns the subject's key as one of the module's key types: [rsa::public_key](../rsa-public_key/README.md),
[p256::public_key](../p256-public_key/README.md), [p384::public_key](../p256-public_key/README.md),
[p521::public_key](../p256-public_key/README.md) or
[ed25519::public_key](../ed25519-public_key/README.md), by its `kind()`; `key_kind::none` for an algorithm the module has no
type for, or a key its type refuses. Its `algorithm()` names the SubjectPublicKeyInfo's algorithm whatever the kind.

## Parameters

None.

## Return value

The [public_key](../x509-public_key/README.md).

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

    auto& key = cert.public_key();
    println("{} {}", key.kind() == crypto::x509::key_kind::rsa, key.algorithm());
    println("{} bits", key.rsa().bits());
}
```

Output:

```text
true 1.2.840.113549.1.1.1
2048 bits
```

## See also

- [raw_subject_public_key_info](raw_subject_public_key_info.md): the key as encoded
- [sgcl::crypto::x509::certificate](README.md)
