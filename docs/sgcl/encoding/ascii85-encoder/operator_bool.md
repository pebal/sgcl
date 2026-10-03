[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85/README.md) › [encoder](README.md)

# sgcl::encoding::ascii85::encoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed encoder, `true` for one made by
[encoder_to](../ascii85/encoder_to.md) and its copies.

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
    encoding::ascii85::encoder a85;
    println("{}", bool(a85));
    io::buffer out;
    a85 = encoding::ascii85::encoder_to(out);
    println("{}", bool(a85));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](ascii85-encoder.md): an encoder that holds none
- [sgcl::encoding::ascii85::encoder](README.md)
