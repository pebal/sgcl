[sgcl](../../README.md) › [hash](../README.md) › [fnv128](README.md)

# sgcl::hash::fnv128::resume

```cpp
static fnv128 resume(const array<byte, 16>& value) noexcept;
```

Makes a hasher that goes on from `value`, the hash of the bytes that came before: the state of an FNV is its value,
so what the hasher takes then continues those bytes, and its [value](value.md) is the hash of them all. Go restores
a saved state through `UnmarshalBinary`. A value is not a seed: `fnv128::of(data, v)` does not compile.

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
    auto h = hash::fnv128::resume(hash::fnv128::of("foo"));
    h.update("bar");
    println("{}", encoding::hex::encode(h.value()));
    println("{}", h.value() == hash::fnv128::of("foobar"));
}
```

Output:

```text
7896bfea9c3c64bf6dc58353d2c293aa
true
```

## See also

- [(constructor)](fnv128.md): a hasher of no bytes yet
- [sgcl::hash::fnv128](README.md)
