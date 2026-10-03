[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::certificates

```cpp
const crypto::x509::chain& certificates() const noexcept;
```

Returns the certificate chain of the identity, the leaf first, as its PEM gave it: what a server sends in its
`Certificate` message. The key is not given out.

## Parameters

None.

## Return value

The chain, a [crypto::x509::chain](../../../crypto/x509.md), held by the identity and shared by its copies.

## Complexity

Constant.

## Exceptions

None.

## Example

A leaf with its CA after it, a chain of two:

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    string leaf_pem = io::read_text("tests/net/tls_testdata/ed25519.pem");
    string ca_pem = io::read_text("tests/net/tls_testdata/ca.pem");
    net::tls::identity server(leaf_pem + ca_pem,
                              crypto::read_secret("tests/net/tls_testdata/ed25519.key"));
    const crypto::x509::chain& sent = server.certificates();
    println("{}", sent.size());
    println("{} {}", sent[0].dns_names(), sent[0].is_ca());
    println("{}", sent[1].is_ca());
}
```

Output:

```text
2
["localhost"] false
true
```

## See also

- [from_pem](from_pem.md): where the chain comes from
- [state](../state.md): the chain a client was sent
- [sgcl::net::tls::identity](README.md)
