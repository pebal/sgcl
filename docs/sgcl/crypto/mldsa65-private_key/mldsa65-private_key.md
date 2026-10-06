[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::private_key

```cpp
private_key(private_key&& other) noexcept;    // (1)
private_key(const private_key&) = delete;     // (2)
```

1. Takes the key of `other` over: the expanded key, in plain memory of its own, changes owner and `other` holds no
   key after: every operation on it throws `std::logic_error` until a key is assigned to it.
2. There is no copy: a second key of the same seed is made by name, [clone](clone.md).

A key is made by [generate](generate.md) or [from_seed](from_seed.md); there is no other public constructor. The move
assignment does the same as (1).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

## Complexity

Constant: the expanded key is not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    auto moved = std::move(key);
    println("{}", moved.sign("x").size());
}
```

Output:

```text
3309
```

## See also

- [clone](clone.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
