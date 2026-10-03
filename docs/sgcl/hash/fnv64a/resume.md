[sgcl](../../README.md) › [hash](../README.md) › [fnv64a](../fnv64a.md)

# sgcl::hash::fnv64a::resume

```cpp
static fnv64a resume(uint64_t value) noexcept;
```

Makes a hasher that goes on from `value`, the hash of the bytes that came before: the state of an FNV is its value,
so what the hasher takes then continues those bytes, and its [value](value.md) is the hash of them all. Go restores
a saved state through `UnmarshalBinary`. A value is not a seed: `fnv64a::of(data, v)` does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the hash of the bytes before, as `value()` gave it |

## Return value

A hasher whose `value()` is `value` and whose updates go on from it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto h = hash::fnv64a::resume(hash::fnv64a::of("foo"));
    h.update("bar");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::fnv64a::of("foobar"));
}
```

Output:

```text
85944171f73967e8
true
```

## See also

- [(constructor)](fnv64a.md): a hasher of no bytes yet
- [sgcl::hash::fnv64a](../fnv64a.md)
