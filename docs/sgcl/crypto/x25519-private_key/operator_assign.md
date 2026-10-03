[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::operator=

```cpp
/*(1)*/ private_key& operator=(private_key&& other) noexcept;
/*(2)*/ private_key& operator=(const private_key&) = delete;
```

1. Takes the key of `other` over, in place of this key, and zeroes it in `other` with stores the compiler cannot
   drop. A key moved from becomes a key again by this assignment. Assigned to itself, a key is left as it is.
2. A key is not copied by an assignment: a second one is asked for by name, with [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Alice, in place of a generated one
    auto key = crypto::x25519::private_key::generate();
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    key = std::move(alice.value());
    println("{}", encoding::hex::encode(key.public_key().bytes()));
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
```

## See also

- [(constructor)](x25519-private_key.md): the same for a construction
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
