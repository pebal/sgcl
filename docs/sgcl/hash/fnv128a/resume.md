[sgcl](../../README.md) › [hash](../README.md) › [fnv128a](README.md)

# sgcl::hash::fnv128a::resume

```cpp
static fnv128a resume(const array<byte, 16>& value) noexcept;
```

Makes a hasher that goes on from `value`, the hash of the bytes that came before: the state of an FNV is its value,
so what the hasher takes then continues those bytes, and its [value](value.md) is the hash of them all. Go restores
a saved state through `UnmarshalBinary`. A value is not a seed: `fnv128a::of(data, v)` does not compile.

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
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto h = hash::fnv128a::resume(hash::fnv128a::of("foo"));
    h.update("bar");
    println("{}", encoding::hex::encode(h.value()));
    println("{}", h.value() == hash::fnv128a::of("foobar"));
}
```

Output:

```text
343e1662793c64bf6f0d3597ba446f18
true
```

## See also

- [(constructor)](fnv128a.md): a hasher of no bytes yet
- [sgcl::hash::fnv128a](README.md)
