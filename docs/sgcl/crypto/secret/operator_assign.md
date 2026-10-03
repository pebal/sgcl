[sgcl](../../README.md) › [crypto](../README.md) › [secret](../secret.md)

# sgcl::crypto::secret\<N\>::operator=

```cpp
/*(1)*/ secret& operator=(secret&& other) noexcept;
/*(2)*/ secret& operator=(const secret&) = delete;
```

1. Takes the bytes of `other` over, in place of this secret's own, and zeroes them in `other` with stores the
   compiler cannot drop. Assigned to itself, a secret is left as it is.
2. A secret is not copied by an assignment: a copy is asked for by name, with [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the secret whose bytes are taken over |

## Return value

`*this`.

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
    // RFC 8032's TEST 1 and TEST 2 seeds
    auto first = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    auto second = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb"));

    crypto::secret<32> seed = first->seed();
    crypto::secret<32> next = second->seed();
    seed = std::move(next);

    array<byte, 32> zeros{};
    println("{} {}", seed == second->seed(), crypto::constant_time::equal(next, zeros));
}
```

Output:

```text
true true
```

## See also

- [(constructor)](secret.md): the same for a construction
- [clone](clone.md): a copy by name
- [sgcl::crypto::secret\<N\>](../secret.md)
