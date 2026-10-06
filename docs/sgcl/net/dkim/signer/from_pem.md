[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::from_pem

```cpp
static expected<signer, io::error> from_pem(const string& domain, const string& selector,
                                            const slice<const byte>& key_pem) noexcept;
```

A signer of the domain and the selector with the key of the PEM text, as the [constructor](signer.md) makes one, a
failure given back rather than thrown: its first private key block, `PRIVATE KEY` (PKCS #8, of RSA or Ed25519) or
`RSA PRIVATE KEY` (PKCS #1); a key of another kind (an EC key) is `crypto::errc::unsupported`.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | the signing domain, `d=` |
| `selector` | the selector, `s=` |
| `key_pem` | the private key in PEM |

## Return value

The signer; an `io::error` of operation `"dkim"`: `net::errc::invalid_address` for a domain or a selector that is
no name, `crypto::errc::malformed` for a text without a key block or a key that does not read,
`crypto::errc::unsupported` for an encrypted key or a key of another kind.

## Complexity

Linear in the text, and the key's checks.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto ec = crypto::p256::private_key::generate();
    auto s = net::dkim::signer::from_pem("example.com", "s1", ec.to_pem());
    println("{}", s.error().message());
}
```

Output:

```text
dkim a private key of neither RSA nor Ed25519: unsupported algorithm or parameter
```

## See also

- [(constructor)](signer.md)
- [signer](README.md)
