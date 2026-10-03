[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [encoder](../hex-encoder.md)

# sgcl::encoding::hex::encoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed encoder, `true` for one made by
[encoder_to](../hex/encoder_to.md) and its copies.

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
    encoding::hex::encoder digits;
    println("{}", bool(digits));
    io::buffer out;
    digits = encoding::hex::encoder_to(out);
    println("{}", bool(digits));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](hex-encoder.md): an encoder that holds none
- [sgcl::encoding::hex::encoder](../hex-encoder.md)
