[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [dumper](../hex-dumper.md)

# sgcl::encoding::hex::dumper::is_closed

```cpp
bool is_closed() const noexcept;
```

Whether the dumper was [closed](close.md), by this handle or a copy of it: `true` after `close()` or
`async_close()`, whether it succeeded or not.

## Parameters

None.

## Return value

`true` when the dumper was closed.

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
    encoding::hex::dumper wire = encoding::hex::dumper_to(out);
    println("{}", wire.is_closed());
    wire.close();
    println("{}", wire.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close, async_close](close.md): the end of the dumper
- [sgcl::encoding::hex::dumper](../hex-dumper.md)
