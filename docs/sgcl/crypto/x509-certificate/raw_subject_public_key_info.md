[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::raw_subject_public_key_info

```cpp
slice<const byte> raw_subject_public_key_info() const noexcept;
```

Returns the SubjectPublicKeyInfo, the subject's key with its algorithm, as the bytes of its encoding: what a key is
pinned by (the SHA-256 of these bytes is HPKP's and many a mobile app's pin), and what the readers of the keys take,
such as [rsa::public_key::from_pkix_der](../rsa-public_key/from_pkix_der.md).

## Parameters

None.

## Return value

The DER of the SubjectPublicKeyInfo.

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

    auto spki = cert.raw_subject_public_key_info();
    println("pin {}", encoding::base64::standard.encode(crypto::sha256::of(spki)));
    auto key = crypto::rsa::public_key::from_pkix_der(spki);
    println("{}", key.value() == cert.public_key().rsa());
}
```

Output:

```text
pin BGX+Lc83i7rKzWyVruiZUqDln5xq+uiKzz5UC1cLvT4=
true
```

## See also

- [public_key](public_key.md): the key, read
- [sgcl::crypto::x509::certificate](README.md)
