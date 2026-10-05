[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::operator== (sgcl::encoding::quoted_printable::encoder)

```cpp
friend bool operator==(const encoder& a, const encoder& b) noexcept;
```

Whether `a` and `b` are handles of one stream; two streams made alike are not equal.

## Parameters

| Parameter | Description |
|---|---|
| `a, b` | the encoders |

## Return value

`true` when both hold the same stream, or neither holds one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    io::buffer b;
    auto one = encoding::quoted_printable::standard.encoder_to(b);
    auto two = encoding::quoted_printable::standard.encoder_to(b);
    auto copy = one;
    println("{} {}", one == copy, one == two);
}
```

Output:

```text
true false
```

## See also

- [encoder](README.md)
