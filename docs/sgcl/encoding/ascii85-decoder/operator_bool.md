[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85/README.md) › [decoder](README.md)

# sgcl::encoding::ascii85::decoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed decoder, `true` for one made by
[decoder_from](../ascii85/decoder_from.md) and its copies.

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
    encoding::ascii85::decoder plain;
    println("{}", bool(plain));
    io::buffer text;
    plain = encoding::ascii85::decoder_from(text);
    println("{}", bool(plain));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](ascii85-decoder.md): a decoder that holds none
- [sgcl::encoding::ascii85::decoder](README.md)
