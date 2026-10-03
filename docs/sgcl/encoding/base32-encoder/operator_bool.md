[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32/README.md) › [encoder](README.md)

# sgcl::encoding::base32::encoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed encoder, `true` for one made by
[encoder_to](../base32/encoder_to.md) and its copies.

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
    encoding::base32::encoder b32;
    println("{}", bool(b32));
    io::buffer out;
    b32 = encoding::base32::standard.encoder_to(out);
    println("{}", bool(b32));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](base32-encoder.md): an encoder that holds none
- [sgcl::encoding::base32::encoder](README.md)
