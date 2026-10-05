[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::encoder::is_closed

```cpp
bool is_closed() const noexcept;
```

Whether [close](close.md) was called.

## Parameters

None.

## Return value

`true` after `close()`.

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
    io::buffer out;
    auto enc = encoding::quoted_printable::standard.encoder_to(out);
    println("{}", enc.is_closed());
    enc.close();
    println("{}", enc.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md)
- [encoder](README.md)
