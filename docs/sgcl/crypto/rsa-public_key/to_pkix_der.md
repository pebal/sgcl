[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::to_pkix_der

```cpp
vector<byte> to_pkix_der() const;
```

Writes the key as a SubjectPublicKeyInfo (`PUBLIC KEY` in PEM): the `rsaEncryption` identifier with NULL parameters
and the RSAPublicKey in a BIT STRING, as Go's `x509.MarshalPKIXPublicKey` and OpenSSL write it, byte for byte: the
bytes a certificate holds for the key.

## Parameters

None.

## Return value

The DER of the SubjectPublicKeyInfo.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto der = key.public_key().to_pkix_der();
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // the test key's certificate
    auto cert = crypto::x509::certificate::from_pem(text);
    println("{}", der == vector<byte>(cert->raw_subject_public_key_info()));
    println("{}", encoding::pem("PUBLIC KEY", der).to_string().view().substr(0, 26));
}
```

Output:

```text
true
-----BEGIN PUBLIC KEY-----
```

## See also

- [from_pkix_der](from_pkix_der.md): reads it back
- [encoding::pem](../../encoding/pem/README.md): the PEM of the encoding
- [sgcl::crypto::rsa::public_key](README.md)
