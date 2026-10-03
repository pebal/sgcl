[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::padded

```cpp
constexpr bool padded() const noexcept;
```

Whether the codec pads its last group to four characters: `true` for `standard` and `url`, and for a codec made
with a padding character; `false` for `raw_standard`, `raw_url` and a codec made by
[without_padding](without_padding.md) or with `nullopt`.

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
    println("{} {}", encoding::base64::url.padded(), encoding::base64::raw_url.padded());
}
```

Output:

```text
true false
```

## See also

- [without_padding](without_padding.md): the same codec without padding
- [sgcl::encoding::base64](../base64.md)
