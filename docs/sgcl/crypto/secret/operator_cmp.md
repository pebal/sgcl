[sgcl](../../README.md) › [crypto](../README.md) › [secret](README.md)

# sgcl::crypto::operator== (sgcl::crypto::secret\<N\>)

```cpp
friend bool operator==(const secret& a, const secret& b) noexcept;
```

Compares the bytes of two secrets of one length in constant time, as [constant_time::equal](../constant_time/equal.md)
does: every byte of both is read whatever they hold, so the time of the comparison tells nothing of how many leading
bytes were equal. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the secrets compared |

## Return value

Whether the secrets hold the same bytes.

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
    // RFC 7748 §6.1's keys: both sides compute the same secret
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    auto bob = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));

    auto ours = alice->shared_secret(bob->public_key()).value();
    auto theirs = bob->shared_secret(alice->public_key()).value();
    println("{} {}", ours == theirs, alice->bytes() != bob->bytes());
}
```

Output:

```text
true true
```

## See also

- [constant_time::equal](../constant_time/equal.md): the comparison of any bytes
- [sgcl::crypto::secret\<N\>](README.md)
