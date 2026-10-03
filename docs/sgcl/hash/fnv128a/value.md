[sgcl](../../README.md) › [hash](../README.md) › [fnv128a](README.md)

# sgcl::hash::fnv128a::value

```cpp
array<byte, 16> value() const noexcept;
```

The hash of the bytes hashed so far, sixteen bytes, the most significant first: the same as [digest](digest.md),
since there is no 128-bit integer type to hand back. It ends nothing: [update](update.md) may go on after it, as
after Go's `h.Sum(nil)`. The value of nothing is the offset basis.

## Parameters

None.

## Return value

The hash, an `array<byte, 16>`.

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
    hash::fnv128a h;
    h.update("foo");
    println("{}", encoding::hex::encode(h.value()));
    h.update("bar");
    println("{}", encoding::hex::encode(h.value()));
}
```

Output:

```text
a68d5ed15f8b5822836dbc79768d78bf
343e1662793c64bf6f0d3597ba446f18
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::fnv128a](README.md)
