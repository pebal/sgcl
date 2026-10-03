[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md) › [encoder](../base64-encoder.md)

# sgcl::encoding::base64::encoder::is_closed

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
    encoding::base64::encoder armored = encoding::base64::standard.encoder_to(out);
    println("{}", armored.is_closed());
    armored.close();
    println("{}", armored.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md): the last group
- [sgcl::encoding::base64::encoder](../base64-encoder.md)
