[sgcl](../../README.md) › [hash](../README.md) › [fnv128](../fnv128.md)

# sgcl::hash::fnv128::value

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
    hash::fnv128 h;
    h.update("foo");
    println("{}", encoding::hex::encode(h.value()));
    h.update("bar");
    println("{}", encoding::hex::encode(h.value()));
}
```

Output:

```text
a68bb298318b5822836dbc78c6a7b1cb
7896bfea9c3c64bf6dc58353d2c293aa
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::fnv128](../fnv128.md)
