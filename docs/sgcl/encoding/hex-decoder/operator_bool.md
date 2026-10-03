[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [decoder](../hex-decoder.md)

# sgcl::encoding::hex::decoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed decoder, `true` for one made by
[decoder_from](../hex/decoder_from.md) and its copies.

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
    encoding::hex::decoder plain;
    println("{}", bool(plain));
    io::buffer text;
    plain = encoding::hex::decoder_from(text);
    println("{}", bool(plain));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](hex-decoder.md): a decoder that holds none
- [sgcl::encoding::hex::decoder](../hex-decoder.md)
