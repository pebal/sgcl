[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md) › [encoder](README.md)

# sgcl::encoding::hex::encoder::is_closed

```cpp
bool is_closed() const noexcept;
```

Whether the encoder was [closed](close.md), by this handle or a copy of it: `true` after `close()` or
`async_close()`, whether it succeeded or not.

## Parameters

None.

## Return value

`true` when the encoder was closed.

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
    encoding::hex::encoder digits = encoding::hex::encoder_to(out);
    println("{}", digits.is_closed());
    digits.close();
    println("{}", digits.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md): the end of the encoder
- [sgcl::encoding::hex::encoder](README.md)
