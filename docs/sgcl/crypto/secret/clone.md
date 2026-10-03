[sgcl](../../README.md) › [crypto](../README.md) › [secret](README.md)

# sgcl::crypto::secret\<N\>::clone

```cpp
secret clone() const noexcept;
```

Returns a second secret of the same bytes. A secret has no copy constructor, so that every copy of it is one the
program asked for by name: one more place the bytes lie, zeroed when that secret goes in turn.

## Parameters

None.

## Return value

A secret of the same `N` bytes.

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
    // RFC 7748 §6.1's keys
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    auto bob = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));

    crypto::secret<32> shared = alice->shared_secret(bob->public_key()).value();
    crypto::secret<32> copy = shared.clone();
    println("{} {}", copy == shared, copy.bytes().data() != shared.bytes().data());
}
```

Output:

```text
true true
```

## See also

- [(constructor)](secret.md): the move
- [sgcl::crypto::secret\<N\>](README.md)
