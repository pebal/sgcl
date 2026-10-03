[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md) › [decoder](../base32-decoder.md)

# sgcl::encoding::base32::decoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed decoder, `true` for one made by
[decoder_from](../base32/decoder_from.md) and its copies.

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
    encoding::base32::decoder plain;
    println("{}", bool(plain));
    io::buffer text;
    plain = encoding::base32::hex.decoder_from(text);
    println("{}", bool(plain));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](base32-decoder.md): a decoder that holds none
- [sgcl::encoding::base32::decoder](../base32-decoder.md)
