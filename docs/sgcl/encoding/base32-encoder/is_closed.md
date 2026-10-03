[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32/README.md) › [encoder](README.md)

# sgcl::encoding::base32::encoder::is_closed

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
    encoding::base32::encoder b32 = encoding::base32::standard.encoder_to(out);
    println("{}", b32.is_closed());
    b32.close();
    println("{}", b32.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md): the last group
- [sgcl::encoding::base32::encoder](README.md)
