[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::generate

```cpp
static private_key generate(key_type type = key_type::ed25519, size_t rsa_bits = 3072);
```

A new key from `crypto::random`: Ed25519 by default, ECDSA on P-256 or P-384, or RSA of `rsa_bits` (2048 to 16384,
ssh-keygen's 3072 by default). Its comment is empty ([with_comment](with_comment.md)).

## Parameters

| Parameter | Description |
|---|---|
| `type` | the kind of key ([key_type](../key_type.md)) |
| `rsa_bits` | an RSA key's size in bits |

## Return value

The key.

## Complexity

Constant for Ed25519 and ECDSA; an RSA key's primes are searched for (a second or so for 3072 bits).

## Exceptions

`std::invalid_argument` for `rsa_bits` outside 2048 to 16384 when the type is RSA.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    for (auto t : {net::ssh::key_type::ed25519, net::ssh::key_type::ecdsa_p384, net::ssh::key_type::rsa}) {
        net::ssh::private_key key = net::ssh::private_key::generate(t, 2048);
        println("{}", key.public_key().type_name());
    }
}
```

Output:

```text
ssh-ed25519
ecdsa-sha2-nistp384
ssh-rsa
```

## See also

- [key_type](../key_type.md)
- [save](save.md)
- [sgcl::net::ssh::private_key](README.md)
