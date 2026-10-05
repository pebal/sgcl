[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [decoder](README.md)

# sgcl::encoding::quoted_printable::decoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream.

## Parameters

None.

## Return value

`true` for a decoder a codec made, `false` for a default-constructed one.

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
    encoding::quoted_printable::decoder none;
    println("{} {}", bool(none), bool(encoding::quoted_printable::standard.decoder_from(b)));
}
```

Output:

```text
false true
```

## See also

- [decoder](README.md)
