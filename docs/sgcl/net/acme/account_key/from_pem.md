[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::from_pem

```cpp
static expected<account_key, io::error> from_pem(const slice<const byte>& pem) noexcept;
```

The key of a PEM text: the first key block of PKCS #8 (`PRIVATE KEY`, of the four kinds), SEC 1 (`EC PRIVATE KEY`) or
PKCS #1 (`RSA PRIVATE KEY`), read where its bytes lie: a `secret_bytes` of [crypto::read_secret](../../../crypto/read_secret.md),
so that the key never passes through managed memory. An RSA key under 2048 bits is refused.

## Parameters

| Parameter | Description |
|---|---|
| `pem` | the PEM text's bytes |

## Return value

The key, or an `io::error` of op `"acme key"`: `crypto::errc::malformed` for a text without a key block, a key of
another kind (X25519, P-521) or an RSA key under 2048 bits; `crypto::errc::unsupported` for an encrypted key.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key;
    auto pem = key.to_pem();
    auto again = net::acme::account_key::from_pem(pem);
    println("{}", *again == key);
    auto none = net::acme::account_key::from_pem(string("no key"));
    println("{}", none.error().code() == crypto::errc::malformed);
}
```

Output:

```text
true
true
```

## See also

- [to_pem](to_pem.md)
- [sgcl::net::acme::account_key](README.md)
