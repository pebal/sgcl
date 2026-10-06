[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::encode

```cpp
static vector<byte> encode(const x509::signing_key& key, const x509::chain& chain, const slice<const byte>& password);
static vector<byte> encode(const x509::signing_key& key, const x509::chain& chain, const slice<const byte>& password,
                           const options& o);
```

A file of the key and its chain, as OpenSSL 3 writes one by default: the certificates and the key each under PBES2
(PBKDF2-HMAC-SHA-256, AES-256-CBC, a random salt and IV), a MAC of HMAC-SHA-256 under the key of RFC 7292 Appendix
B, the leaf and the key tied by a localKeyId (the SHA-1 of the leaf, as OpenSSL ties them) and both named by
`o.friendly_name`. The key is the module's of any kind it signs with — `p256`, `p384`, `p521::private_key`,
`ed25519::private_key`, `rsa::private_key` — taken through [x509::signing_key](../x509-signing_key/README.md); its
PKCS #8 goes from plain memory into the cipher. OpenSSL, Windows, Java and browsers read the file.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the private key |
| `chain` | its certificate first, then the ones above it; empty for the key alone |
| `password` | the password: bytes or text |
| `o` | `friendly_name`, `iterations` ([options](../pkcs12-options.md)) |

## Return value

The file's DER.

## Complexity

Linear in the size of the chain, besides three derivations of `o.iterations` iterations.

## Exceptions

`std::invalid_argument` for a leaf that is not the key's certificate and for iterations of 0.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a key and its certificate, written to a file under a password
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template t;
    t.common_name = "example.test";
    crypto::x509::chain chain;
    chain.push_back(crypto::x509::create_certificate(t, key));
    auto file = crypto::pkcs12::encode(key, chain, "password", {.friendly_name = "example"});
    println("{}", crypto::pkcs12::parse(file, "password")->key_kind() == crypto::x509::key_kind::p256);
}
```

Output:

```text
true
```

## See also

- [parse](parse.md)
- [sgcl::crypto::pkcs12](README.md)
