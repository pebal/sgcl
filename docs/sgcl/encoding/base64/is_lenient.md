[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::is_lenient

```cpp
constexpr bool is_lenient() const noexcept;
```

Whether the codec's decoding is lenient: `false` for the constants and a codec made by the constructor, `true`
for one made by [lenient](lenient.md).

## Parameters

None.

## Return value

`true` when the decoding skips line endings and takes bits past the data.

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
    auto mime = encoding::base64::standard.lenient();
    println("{} {}", encoding::base64::standard.is_lenient(), mime.is_lenient());
}
```

Output:

```text
false true
```

## See also

- [lenient](lenient.md): the same codec, its decoding lenient
- [sgcl::encoding::base64](../base64.md)
