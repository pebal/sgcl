[sgcl](../../README.md) › [crypto](../README.md) › [secret](../secret.md)

# sgcl::crypto::secret\<N\>::secret

```cpp
/*(1)*/ secret(secret&& other) noexcept;
/*(2)*/ secret(const secret&) = delete;
```

1. Takes the bytes of `other` over and zeroes them in `other`, with stores the compiler cannot drop: the bytes stay
   in one place.
2. A secret is not copied by a constructor: a copy is asked for by name, with [clone](clone.md).

A program gets a secret from the module (a shared secret, a private key's bytes); there is no other public
constructor. The destructor, `~secret()`, zeroes the bytes the same way.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the secret whose bytes are taken over |

## Complexity

Linear in `N`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 seed
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    crypto::secret<32> seed = key->seed();
    crypto::secret<32> moved(std::move(seed));

    array<byte, 32> zeros{};
    println("{}", crypto::constant_time::equal(seed, zeros));
    println("{}", moved == key->seed());
}
```

Output:

```text
true
true
```

## See also

- [operator=](operator_assign.md): the same for an assignment
- [clone](clone.md): a copy by name
- [sgcl::crypto::secret\<N\>](../secret.md)
