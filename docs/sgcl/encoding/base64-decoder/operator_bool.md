[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64/README.md) › [decoder](README.md)

# sgcl::encoding::base64::decoder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a stream: `false` for a default-constructed decoder, `true` for one made by
[decoder_from](../base64/decoder_from.md) and its copies.

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
    encoding::base64::decoder plain;
    println("{}", bool(plain));
    io::buffer text;
    plain = encoding::base64::url.decoder_from(text);
    println("{}", bool(plain));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](base64-decoder.md): a decoder that holds none
- [sgcl::encoding::base64::decoder](README.md)
