[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::private_key_pem

```cpp
crypto::secret_bytes private_key_pem() const;
```

The private key as PEM, `PRIVATE KEY` over its PKCS #8, as OpenSSL writes it: what a signer made by
[generate](generate.md) is kept as, and read back by the [constructor](signer.md). A
[secret_bytes](../../../crypto/secret_bytes/README.md), never managed memory, zeroed when it goes.

## Parameters

None.

## Return value

The PEM text.

## Complexity

Linear in the key.

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
    auto s = net::dkim::signer::generate("example.com", "s1", net::dkim::algorithm::ed25519_sha256);
    crypto::secret_bytes pem = s.private_key_pem();
    println("{} bytes", pem.size());
    net::dkim::signer again("example.com", "s1", pem);
    println("{}", again.record() == s.record());
}
```

Output:

```text
119 bytes
true
```

## See also

- [generate](generate.md), [(constructor)](signer.md)
- [signer](README.md)
