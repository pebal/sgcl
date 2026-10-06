[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::parse

```cpp
static expected<pkcs12, error> parse(const slice<const byte>& file, const slice<const byte>& password) noexcept;
static expected<pkcs12, error> parse(const slice<const byte>& file, const slice<const byte>& password,
                                     const options& o) noexcept;
```

The contents of a PKCS #12 file (DER, or the BER of older writers) under its password: the MAC checked first, over
the whole of what it covers, then every part decrypted and read. The password is the bytes given, read where they lie
(a [secret_bytes](../secret_bytes/README.md) of [read_secret](../read_secret.md), a string); the MAC of RFC 7292 takes
it as UTF-16 (a password that is not UTF-8 byte by byte, as OpenSSL takes it), PBES2 as it is. What is read: PBES2
with PBKDF2 (HMAC-SHA-1 to SHA-512) and AES-128/192/256-CBC, the MAC of RFC 7292 (HMAC of SHA-1 to SHA-512) and
PBMAC1 (RFC 9579), parts not encrypted, a file without a MAC; keys of the kinds the module has, certificates, their
friendlyName and localKeyId. The leaf is the key's certificate by its localKeyId, else by its public key; the chain
follows it, each certificate's issuer after it.

## Parameters

| Parameter | Description |
|---|---|
| `file` | the file's bytes |
| `password` | the password: bytes or text |
| `o` | `max_iterations` ([options](../pkcs12-options.md)) |

## Return value

The file read, or an error:

- `errc::authentication`: a MAC that does not verify, a part that does not decrypt — a wrong password;
- `errc::unsupported`: a legacy PBE of PKCS #12 (3DES, RC2, RC4, OpenSSL's `-legacy`), a public-key integrity mode,
  a key of a kind the module does not have, a derivation of more than `max_iterations`, named in the message;
- `errc::malformed`: bytes that do not read.

## Complexity

Linear in the size of the file, besides the derivations: three of the file's iterations each (2048 by OpenSSL's default).

## Exceptions

None.

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

    auto read = crypto::pkcs12::parse(file, "password");
    println("{} certificate(s), {}", read->certificates().size(), read->friendly_name());
    auto wrong = crypto::pkcs12::parse(file, "Password");
    println("{}", wrong.error().message());
}
```

Output:

```text
1 certificate(s), example
sgcl::crypto::pkcs12: the MAC does not verify (a wrong password?)
```

## See also

- [encode](encode.md)
- [net::tls::identity::from_pkcs12](../../net/tls/identity/from_pkcs12.md)
- [sgcl::crypto::pkcs12](README.md)
