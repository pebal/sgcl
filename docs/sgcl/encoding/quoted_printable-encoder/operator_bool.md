[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::encoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream.

## Parameters

None.

## Return value

`true` for a encoder a codec made, `false` for a default-constructed one.

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
    encoding::quoted_printable::encoder none;
    println("{} {}", bool(none), bool(encoding::quoted_printable::standard.encoder_to(b)));
}
```

Output:

```text
false true
```

## See also

- [encoder](README.md)
