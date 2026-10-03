[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64/README.md) › [encoder](README.md)

# sgcl::encoding::base64::encoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed encoder, `true` for one made by
[encoder_to](../base64/encoder_to.md) and its copies.

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
    encoding::base64::encoder armored;
    println("{}", bool(armored));
    io::buffer out;
    armored = encoding::base64::standard.encoder_to(out);
    println("{}", bool(armored));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](base64-encoder.md): an encoder that holds none
- [sgcl::encoding::base64::encoder](README.md)
