[sgcl](../../README.md) › [hash](../README.md) › [fnv64](README.md)

# sgcl::hash::fnv64::resume

```cpp
static fnv64 resume(uint64_t value) noexcept;
```

Makes a hasher that goes on from `value`, the hash of the bytes that came before: the state of an FNV is its value,
so what the hasher takes then continues those bytes, and its [value](value.md) is the hash of them all. Go restores
a saved state through `UnmarshalBinary`. A value is not a seed: `fnv64::of(data, v)` does not compile.

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
    auto h = hash::fnv64::resume(hash::fnv64::of("foo"));
    h.update("bar");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::fnv64::of("foobar"));
}
```

Output:

```text
340d8765a4dda9c2
true
```

## See also

- [(constructor)](fnv64.md): a hasher of no bytes yet
- [sgcl::hash::fnv64](README.md)
