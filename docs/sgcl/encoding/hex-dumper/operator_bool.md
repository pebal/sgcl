[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md) › [dumper](README.md)

# sgcl::encoding::hex::dumper::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed dumper, `true` for one made by
[dumper_to](../hex/dumper_to.md) and its copies.

## Parameters

None.

## Return value

`true` when the handle holds a stream.

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
    encoding::hex::dumper wire;
    println("{}", bool(wire));
    io::buffer out;
    wire = encoding::hex::dumper_to(out);
    println("{}", bool(wire));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](hex-dumper.md): a dumper that holds none
- [sgcl::encoding::hex::dumper](README.md)
