[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::algorithm

```cpp
dkim::algorithm algorithm() const noexcept;
```

The [algorithm](../algorithm.md) of the key: `rsa_sha256` for an RSA key, `ed25519_sha256` for an Ed25519 key.

## Parameters

None.

## Return value

The algorithm.

## Complexity

Constant.

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
    net::dkim::signer s("example.com", "s1", crypto::ed25519::private_key::generate().to_pem());
    println("{}", s.algorithm() == net::dkim::algorithm::ed25519_sha256);
}
```

Output:

```text
true
```

## See also

- [algorithm](../algorithm.md)
- [signer](README.md)
