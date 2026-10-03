[sgcl](../../README.md) › [hash](../README.md) › [fnv32a](../fnv32a.md)

# sgcl::hash::fnv32a::resume

```cpp
static fnv32a resume(uint32_t value) noexcept;
```

Makes a hasher that goes on from `value`, the hash of the bytes that came before: the state of an FNV is its value,
so what the hasher takes then continues those bytes, and its [value](value.md) is the hash of them all. Go restores
a saved state through `UnmarshalBinary`. A value is not a seed: `fnv32a::of(data, v)` does not compile.

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
    auto h = hash::fnv32a::resume(hash::fnv32a::of("foo"));
    h.update("bar");
    println("{:08x}", h.value());
    println("{}", h.value() == hash::fnv32a::of("foobar"));
}
```

Output:

```text
bf9cf968
true
```

## See also

- [(constructor)](fnv32a.md): a hasher of no bytes yet
- [sgcl::hash::fnv32a](../fnv32a.md)
