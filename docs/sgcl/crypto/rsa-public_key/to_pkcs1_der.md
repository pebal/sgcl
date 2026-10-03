[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::to_pkcs1_der

```cpp
vector<byte> to_pkcs1_der() const;
```

Writes the key as PKCS #1's RSAPublicKey (RFC 8017 §A.1.1, `RSA PUBLIC KEY` in PEM), `SEQUENCE { modulus,
publicExponent }`, as Go's `x509.MarshalPKCS1PublicKey` and OpenSSL write it, byte for byte.

## Parameters

None.

## Return value

The DER of the RSAPublicKey.

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

    auto der = key.public_key().to_pkcs1_der();
    println("{} bytes: {}...", der.size(), encoding::hex::encode(der.as_slice().subslice(0, 8)));
    println("{}", encoding::pem("RSA PUBLIC KEY", der).to_string().view().substr(0, 30));
}
```

Output:

```text
270 bytes: 3082010a02820101...
-----BEGIN RSA PUBLIC KEY-----
```

## See also

- [from_pkcs1_der](from_pkcs1_der.md): reads it back
- [to_pkix_der](to_pkix_der.md): the encoding with the algorithm's identifier
- [sgcl::crypto::rsa::public_key](README.md)
