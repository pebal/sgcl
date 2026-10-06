[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::private_key

```cpp
private_key(private_key&& other) noexcept;    // (1)
private_key(const private_key&) = delete;     // (2)
```

1. Takes the key of `other` over: its bytes are copied into the new object and `other` is zeroed whole, holding no
   key after: every operation on it throws `std::logic_error` until a key is assigned to it.
2. There is no copy: a second key is made by name, [clone](clone.md).

A key is made by [generate](generate.md) or [from_bytes](from_bytes.md); there is no other public constructor. The move
assignment does the same as (1).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

## Complexity

Constant: the key's 4n bytes and the hash streams of PK.seed, under 500 bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto moved = std::move(key);
    println("{}", moved.sign("x").size());
}
```

Output:

```text
7856
```

## See also

- [clone](clone.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
