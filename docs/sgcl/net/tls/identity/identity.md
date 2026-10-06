[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::identity

```cpp
identity(const string& certificate_chain_pem, const slice<const byte>& key_pem);    // (1)
explicit identity(const crypto::pkcs12& file);                                      // (4)
identity(const identity& other) noexcept;                                           // (2), implicitly declared
identity(identity&& other) noexcept;                                                // (3), implicitly declared
```

1. An identity of the chain and the key, each in PEM, as [from_pem](from_pem.md) makes it; a chain or a key it
   refuses is thrown. Go's `tls.X509KeyPair`, and with the files read, `tls.LoadX509KeyPair`.
2. A handle of the same identity as `other`: one chain and one key, shared.
3. The same; `other` still holds the identity, since the move of the word inside is its copy.
4. An identity of a PKCS #12 file read ([crypto::pkcs12](../../../crypto/pkcs12/README.md)), as
   [from_pkcs12](from_pkcs12.md) makes it; a file it refuses is thrown.

There is no default constructor: an identity holds a chain and a key from its making on.

## Parameters

| Parameter | Description |
|---|---|
| `certificate_chain_pem` | every `CERTIFICATE` block of the text, the leaf first |
| `key_pem` | the leaf's private key in PEM, read where it lies: the bytes of [crypto::read_secret](../../../crypto/secret/README.md) |
| `other` | the handle whose identity this one shares |
| `file` | a PKCS #12 file read: its chain and its key |

## Complexity

- (1) Linear in the size of the PEM, and one signature by the key verified under the leaf's public key.
- (2–3) Constant.
- (4) Linear in the size of the chain, and one signature verified.

## Exceptions

- (1) `std::invalid_argument` for what [from_pem](from_pem.md) returns as an error, its message the error's.
- (2–3) None.
- (4) `std::invalid_argument` for what [from_pkcs12](from_pkcs12.md) returns as an error.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

#include <stdexcept>

using namespace sgcl;

int main() {
    net::tls::identity server(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                              crypto::read_secret("tests/net/tls_testdata/ecdsa.key"));
    net::tls::identity copy = server;
    println("{}", &copy.certificates() == &server.certificates());

    try {
        net::tls::identity mixed(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                 crypto::read_secret("tests/net/tls_testdata/rsa.key"));
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
identity the private key is not the leaf certificate's: malformed data
```

## See also

- [from_pem](from_pem.md): the same, an error returned
- [operator=](operator_assign.md): a handle made one of another identity
- [sgcl::net::tls::identity](README.md)
