[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::sign

```cpp
vector<byte> sign(const slice<const byte>& data) const noexcept;
```

The signature blob of data by the key's algorithm (RFC 4253 §6.6): the algorithm's name and the signature, as SSH
carries it — ssh-ed25519, ecdsa-sha2-nistp256, -nistp384 or -nistp521 (a hedged RFC 6979 nonce), rsa-sha2-512. What
[public_key::verify](../public_key/verify.md) checks.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the data signed |

## Return value

The signature blob.

## Complexity

One signature.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::generate();
    vector<byte> sig = key.sign("message");
    println("{} {}", key.public_key().verify("message", sig), key.public_key().verify("other", sig));
}
```

Output:

```text
true false
```

## See also

- [public_key::verify](../public_key/verify.md)
- [sgcl::net::ssh::private_key](README.md)
