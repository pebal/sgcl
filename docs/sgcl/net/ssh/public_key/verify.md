[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& data, const slice<const byte>& signature) const noexcept;
```

Whether `signature`, a signature blob (its algorithm's name and its bytes: what [private_key::sign](../private_key/sign.md)
and an agent give), signs `data` under the key (a certificate's: under the key it certifies). The algorithm must be one
of the key's: ssh-ed25519, ecdsa-sha2-nistp256, -nistp384 or -nistp521, rsa-sha2-256 or rsa-sha2-512; an `ssh-rsa` signature
(SHA-1) is never taken.

## Parameters

| Parameter | Description |
|---|---|
| `data` | what was signed |
| `signature` | the signature blob |

## Return value

Whether it signs the data.

## Complexity

One signature's check.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::load("tests/net/ssh/testdata/rsa");
    vector<byte> sig = key.sign("data");
    println("{} {}", key.public_key().verify("data", sig), key.public_key().verify("data!", sig));
}
```

Output:

```text
true false
```

## See also

- [private_key::sign](../private_key/sign.md)
- [agent::sign](../agent/sign.md)
- [sgcl::net::ssh::public_key](README.md)
