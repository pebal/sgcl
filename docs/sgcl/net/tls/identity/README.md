[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md)

# sgcl::net::tls::identity

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class identity;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::identity` is a certificate chain with its private key, Go's `tls.Certificate` as
`tls.LoadX509KeyPair` and `tls.X509KeyPair` make it: `net::tls::identity(io::read_text(chain_file),
crypto::read_secret(key_file))` is the first. A server's [config](../config.md) holds one or more, and the handshake
chooses the one whose leaf is for the name the client sent, else the first. A client's config holds the ones it
sends to a server that asks for a certificate ([client_auth](../client_auth.md)): the first whose key signs a scheme
the server takes and whose chain one of the authorities it names issued.

It is a handle of one word whose state is made in the constructor, as a [string](../../../core/string/README.md) is: copies
share one chain and one key. The private key sits in an unmanaged block of its own, never copied (copies of the
handle and of a config share it), and is zeroed by the keys' own destructors when the identity is collected. Its
kinds are those TLS 1.3 signs `CertificateVerify` with: Ed25519; ECDSA P-256 with SHA-256; ECDSA P-384 with SHA-384; ECDSA P-521 with SHA-512;
RSA with RSASSA-PSS over SHA-256, SHA-384 or SHA-512 (`rsa_pss_rsae_*`), each where the key holds its digest and salt:
a key of 1024 bits has no room for SHA-512's, and a client that offers that scheme alone is refused with
`handshake_failure`, as one with no scheme in common. The key is checked against the leaf, by a signature verified
under the leaf's public key, before the identity exists.

## Rules

- The key's PEM is bytes read where they lie, and its DER goes straight into a
  [secret_bytes](../../../crypto/secret_bytes/README.md): a key file read with
  [crypto::read_secret](../../../crypto/secret/README.md) never passes through managed memory. A `string` converts too, but
  its bytes are managed; that is the caller's choice.
- There is no empty identity: an object holds a chain and a key from its constructor on, and a moved-from one still
  holds them, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still points.
- The OCSP response a server staples for the leaf (RFC 6066) is the identity's too, shared by its copies: set by the
  program ([set_ocsp_staple](set_ocsp_staple.md), Go's `Certificate.OCSPStaple`), or fetched and refreshed by a
  server whose config has `ocsp_stapling`. It is sent to a client that asks for it (`status_request`), never past its
  `nextUpdate`.
## Member functions

| Function | Description |
|---|---|
| [(constructor)](identity.md) | an identity of a chain and a key in PEM, a broken one thrown; a copy |
| [operator=](operator_assign.md) | makes this handle one of another's identity |
| [from_pem](from_pem.md) | an identity of a chain and a key in PEM, a broken one returned as an error, `static` |
| [from_pkcs12](from_pkcs12.md) | an identity of a PKCS #12 file, its bytes and password or the file read, `static` |

#### Observers

| Function | Description |
|---|---|
| [certificates](certificates.md) | the chain, the leaf first |

#### OCSP stapling

| Function | Description |
|---|---|
| [ocsp_staple](ocsp_staple.md) | the OCSP response a server staples for the leaf now |
| [set_ocsp_staple](set_ocsp_staple.md) | sets the OCSP response a server staples, checked against the leaf, or clears it |

## Example

A server of two identities, both for localhost: the client is given the first.

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config server_cfg;
    for (string kind : {"ed25519", "ecdsa"}) {
        server_cfg.identities.push_back(
            net::tls::identity(io::read_text("tests/net/tls_testdata/" + kind + ".pem"),
                               crypto::read_secret("tests/net/tls_testdata/" + kind + ".key")));
    }
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    crypto::x509::chain sent = net::tls::state_of(*c)->peer_certificates;
    println("{}", sent[0].public_key().kind() == crypto::x509::key_kind::ed25519);
    c->close();
    incoming.close();
}
```

Output:

```text
true
```

## See also

- [config](../config.md): where a server's and a client's identities stand
- [crypto::x509](../../../crypto/x509.md): the certificates; [crypto::secret](../../../crypto/secret/README.md): the key's bytes
- [listen](../listen.md), [server](../server.md): the servers that present it
- [net::tls](../README.md)
