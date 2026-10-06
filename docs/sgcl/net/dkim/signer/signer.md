[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::signer

```cpp
signer(const string& domain, const string& selector, const slice<const byte>& key_pem);    // (1)
signer(const signer& other) noexcept = default;                                            // (2)
```

1. A signer of the domain and the selector with the key of the PEM text: its first private key block, `PRIVATE KEY`
   (PKCS #8, of RSA or Ed25519) or `RSA PRIVATE KEY` (PKCS #1). The text is read where it lies, its key's DER put
   straight into memory that is zeroed and never managed: a file read with
   [crypto::read_secret](../../../crypto/read_secret.md), a buffer; a `string` converts too. What
   [from_pem](from_pem.md) refuses is thrown (DESIGN 234: a key the program holds is constructed, input is parsed).
2. The same signer: the handle copied, the key shared.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | the signing domain, `d=` |
| `selector` | the selector, `s=` |
| `key_pem` | the private key in PEM |
| `other` | the signer to share |

## Complexity

- (1) Linear in the text, and the key's checks.
- (2) Constant.

## Exceptions

- (1) `bad_expected_access<io::error>` with [from_pem](from_pem.md)'s error: no key in the text, a key of another
  kind, an encrypted key, a domain or a selector that is no name.
- (2) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    net::dkim::signer s("Example.COM", "Mail", key.to_pem());
    println("{} {}", s.domain(), s.selector());
    try {
        net::dkim::signer bad("example.com", "mail", string("no key here"));
    } catch (const bad_expected_access<io::error>& e) {
        println("{}", e.error().message());
    }
}
```

Output:

```text
example.com mail
dkim PEM: no private key block: malformed data
```

## See also

- [from_pem](from_pem.md), [generate](generate.md)
- [signer](README.md)
