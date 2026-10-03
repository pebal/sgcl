[sgcl](../../README.md) › [encoding](../README.md) › [base32](README.md)

# sgcl::encoding::base32::padded

```cpp
constexpr bool padded() const noexcept;
```

Whether the codec pads its last group to eight characters: `true` for `standard` and `hex`, and for a codec made
with a padding character; `false` for a codec made by [without_padding](without_padding.md) or with `nullopt`.

## Parameters

None.

## Return value

`true` when the codec has a padding character.

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
    auto raw = encoding::base32::hex.without_padding();
    println("{} {}", encoding::base32::hex.padded(), raw.padded());
}
```

Output:

```text
true false
```

## See also

- [without_padding](without_padding.md): the same codec without padding
- [sgcl::encoding::base32](README.md)
