[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::operator=

```cpp
private_key& operator=(private_key&& other) noexcept;    // (1)
private_key& operator=(const private_key&) = delete;     // (2)
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
    // RFC 8032's TEST 1 key, in place of a generated one
    auto key = crypto::ed25519::private_key::generate();
    auto test1 = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    key = std::move(test1.value());
    println("{}", encoding::hex::encode(key.public_key().bytes()));
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
```

## See also

- [(constructor)](ed25519-private_key.md): the same for a construction
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
