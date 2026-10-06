[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::pkcs12

```cpp
#include "sgcl/crypto/pkcs12.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class pkcs12 {
    public:
        struct options;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::pkcs12` is a PKCS #12 file read (RFC 7292): a private key, its certificate chain and their name in one
file sealed by a password — a `.p12` or `.pfx`, what browsers, Windows, macOS, Java's key stores and `openssl pkcs12`
exchange. [parse](parse.md) reads one under its password; [encode](encode.md) writes one as OpenSSL 3 writes it.
The file's key is the module's, of whichever kind: [signing_key](signing_key.md) gives it to whatever signs,
[key_pkcs8](key_pkcs8.md) to the typed key, and [net::tls::identity](../../net/tls/identity/README.md) takes the file
whole, `net::tls::identity::from_pkcs12(bytes, password)`. Go's standard library has no PKCS #12; this is what
`golang.org/x/crypto/pkcs12` (which reads only the legacy algorithms) and `go-pkcs12` give a Go program.

## Rules

- **A handle of one word**, read once and never changed; the key lies in plain memory of its own, shared by copies,
  zeroed when the last one's state is collected.
- **Secrets**: the password is read where it lies; a part is decrypted into plain memory and the key read from there,
  never through managed memory; the MAC is compared in constant time, the CBC padding checked in constant time.
- **What is read and written**: PBES2 (PBKDF2 with HMAC-SHA-1 to SHA-512, AES-128/192/256-CBC), the MAC of RFC 7292
  (HMAC of SHA-1 to SHA-512) and PBMAC1 (RFC 9579), unencrypted parts and files without a MAC; written, OpenSSL 3's
  defaults (PBKDF2-HMAC-SHA-256 of 2048 iterations, AES-256-CBC, an HMAC-SHA-256 MAC). Not here: the legacy PBEs of
  PKCS #12 (3DES, RC2, RC4: OpenSSL's `-legacy`, the module has none of those ciphers), the public-key integrity and
  privacy modes, SDSI certificates, CRL and secret bags (passed over).
- **Errors** ([parse](parse.md)): a wrong password is `errc::authentication`; what the module does not have
  `errc::unsupported`, named; bytes that do not read `errc::malformed`. A derivation of more iterations than
  `options::max_iterations` (a million) is refused before it runs.

## Member types

| Type | Definition |
|---|---|
| [options](../pkcs12-options.md) | the friendly name and iterations of a file written, the most iterations of one read |

## Member functions

| Function | Description |
|---|---|
| [parse](parse.md) | a file read under its password (static) |
| [encode](encode.md) | a file of a key and its chain (static) |

#### Observers

| Function | Description |
|---|---|
| [certificates](certificates.md) | the chain, the leaf first |
| [friendly_name](friendly_name.md) | the name of the key or the leaf |
| [key_kind](key_kind.md) | the kind of the key |
| [signing_key](signing_key.md) | the key as the module signs with it |
| [key_pkcs8](key_pkcs8.md) | the key's PKCS #8, in plain memory |

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

    // read back: the same key and the chain
    auto read = crypto::pkcs12::parse(file, "password").value();
    println("{} {} certificate(s)", read.friendly_name(), read.certificates().size());
    auto typed = crypto::p256::private_key::from_pkcs8_der(read.key_pkcs8());
    println("{}", typed->public_key() == key.public_key());
}
```

Output:

```text
example 1 certificate(s)
true
```

## See also

- [x509::signing_key](../x509-signing_key/README.md), [x509::certificate](../x509-certificate/README.md)
- [net::tls::identity::from_pkcs12](../../net/tls/identity/from_pkcs12.md)
- [secret_bytes](../secret_bytes/README.md), [read_secret](../read_secret.md)
- [sgcl::crypto](../README.md)
