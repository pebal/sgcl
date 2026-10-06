[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::find

```cpp
optional<jwk> find(const string& kid) const noexcept;
```

The first key whose `kid` is `kid`.

## Parameters

| Parameter | Description |
|---|---|
| `kid` | the kid |

## Return value

The key; `nullopt` when no key has the kid.

## Complexity

Linear in the number of keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk_set set{crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa, {.kid = "a"})};
    println("{} {}", set.find("a").has_value(), set.find("b").has_value());
}
```

Output:

```text
true false
```

## See also

- [jwk::kid](../jose-jwk/kid.md)
- [sgcl::crypto::jose::jwk_set](README.md)
