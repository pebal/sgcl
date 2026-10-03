[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md) › [encoder](../ascii85-encoder.md)

# sgcl::encoding::ascii85::encoder::is_closed

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
    encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(out);
    println("{}", a85.is_closed());
    a85.close();
    println("{}", a85.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md): the last group
- [sgcl::encoding::ascii85::encoder](../ascii85-encoder.md)
